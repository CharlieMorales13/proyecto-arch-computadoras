#include "entradas.h"

#include "config.h"
#include "driver/gpio.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_check.h"
#include "esp_log.h"
#include "pines.h"

static const char *TAG = "entradas";

typedef enum {
    IN_IGNICION = 0,
    IN_SENTIDO,
    IN_DIR_IZQ,
    IN_DIR_DER,
    IN_INTERMITENTES,
    IN_ARRANQUE,
    IN_FRENO_DEL,
    IN_FRENO_TRA,
    IN_CAMBIO,
    IN_RESET,
    IN_TOTAL
} entrada_t;

typedef struct {
    gpio_num_t pin;
    bool pull_up_interna;       /* false en 34..39: llevan 10 kΩ externa */
} entrada_def_t;

static const entrada_def_t DEF[IN_TOTAL] = {
    [IN_IGNICION]      = { PIN_SW_IGNICION,      false },
    [IN_SENTIDO]       = { PIN_SW_SENTIDO,       false },
    [IN_DIR_IZQ]       = { PIN_SW_DIR_IZQ,       false },
    [IN_DIR_DER]       = { PIN_SW_DIR_DER,       false },
    [IN_INTERMITENTES] = { PIN_SW_INTERMITENTES, true  },
    [IN_ARRANQUE]      = { PIN_BTN_ARRANQUE,     true  },
    [IN_FRENO_DEL]     = { PIN_BTN_FRENO_DEL,    true  },
    [IN_FRENO_TRA]     = { PIN_BTN_FRENO_TRA,    true  },
    [IN_CAMBIO]        = { PIN_BTN_CAMBIO,       true  },
    [IN_RESET]         = { PIN_BTN_RESET,        true  },
};

/* Estado del antirrebote de cada entrada */
static bool s_lectura_previa[IN_TOTAL];
static bool s_estable[IN_TOTAL];
static bool s_estable_previo[IN_TOTAL];

/* ADC */
static adc_oneshot_unit_handle_t s_adc;
static adc_channel_t s_canal;
static uint16_t s_muestras[ADC_PROMEDIO];
static uint32_t s_suma;
static uint8_t s_indice;
static bool s_adc_listo;

esp_err_t entradas_iniciar(void)
{
    /* GPIO: dos grupos, con y sin pull-up interna */
    uint64_t con_pullup = 0;
    uint64_t sin_pullup = 0;
    for (int i = 0; i < IN_TOTAL; i++) {
        if (DEF[i].pull_up_interna) {
            con_pullup |= 1ULL << DEF[i].pin;
        } else {
            sin_pullup |= 1ULL << DEF[i].pin;
        }
    }
    gpio_config_t cfg = {
        .pin_bit_mask = con_pullup,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&cfg), TAG, "gpio con pull-up");
    cfg.pin_bit_mask = sin_pullup;
    cfg.pull_up_en = GPIO_PULLUP_DISABLE;
    ESP_RETURN_ON_ERROR(gpio_config(&cfg), TAG, "gpio sin pull-up");

    /* El estado inicial se toma del pin real para no generar flancos falsos
     * (por ejemplo, si la ignición ya está encendida al conectar la placa). */
    for (int i = 0; i < IN_TOTAL; i++) {
        const bool activo = gpio_get_level(DEF[i].pin) == 0;
        s_lectura_previa[i] = activo;
        s_estable[i] = activo;
        s_estable_previo[i] = activo;
    }

    /* ADC1 en modo de una lectura (oneshot) */
    adc_unit_t unidad;
    ESP_RETURN_ON_ERROR(adc_oneshot_io_to_channel(PIN_POT_GASOLINA, &unidad, &s_canal),
                        TAG, "el pin del potenciómetro no tiene ADC");
    const adc_oneshot_unit_init_cfg_t ucfg = {
        .unit_id = unidad,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    ESP_RETURN_ON_ERROR(adc_oneshot_new_unit(&ucfg, &s_adc), TAG, "unidad ADC");
    const adc_oneshot_chan_cfg_t ccfg = {
        .atten = ADC_ATTEN_DB_12,       /* rango de 0 a ~3.1 V */
        .bitwidth = ADC_BITWIDTH_12,    /* 0..4095 */
    };
    ESP_RETURN_ON_ERROR(adc_oneshot_config_channel(s_adc, s_canal, &ccfg), TAG, "canal ADC");

    ESP_LOGI(TAG, "10 entradas digitales y potenciómetro en ADC%d canal %d", unidad + 1, s_canal);
    return ESP_OK;
}

/* Antirrebote: el valor estable cambia cuando dos lecturas seguidas coinciden. */
static void leer_digitales(void)
{
    for (int i = 0; i < IN_TOTAL; i++) {
        const bool lectura = gpio_get_level(DEF[i].pin) == 0;
        s_estable_previo[i] = s_estable[i];
        if (lectura == s_lectura_previa[i]) {
            s_estable[i] = lectura;
        }
        s_lectura_previa[i] = lectura;
    }
}

static bool flanco(entrada_t i)
{
    return s_estable[i] && !s_estable_previo[i];
}

static uint16_t leer_adc_promedio(void)
{
    int crudo = 0;
    if (adc_oneshot_read(s_adc, s_canal, &crudo) != ESP_OK) {
        return (uint16_t)(s_suma / ADC_PROMEDIO);
    }
    if (!s_adc_listo) {
        for (int i = 0; i < ADC_PROMEDIO; i++) {
            s_muestras[i] = (uint16_t)crudo;
        }
        s_suma = (uint32_t)crudo * ADC_PROMEDIO;
        s_adc_listo = true;
    } else {
        s_suma = s_suma - s_muestras[s_indice] + (uint32_t)crudo;
        s_muestras[s_indice] = (uint16_t)crudo;
        s_indice = (s_indice + 1) % ADC_PROMEDIO;
    }
    return (uint16_t)(s_suma / ADC_PROMEDIO);
}

static int adc_a_porcentaje(uint16_t crudo)
{
    if (crudo <= ADC_MIN) {
        return 0;
    }
    if (crudo >= ADC_MAX) {
        return 100;
    }
    return (int)(crudo - ADC_MIN) * 100 / (ADC_MAX - ADC_MIN);
}

/* Sube de nivel solo si se supera el umbral por el margen de histéresis y
 * baja solo si se cae por debajo del umbral menos el margen. */
static uint8_t calcular_nivel(uint8_t nivel, int pct)
{
    static const int UMBRAL[GAS_NIVEL_MAX + 1] = GAS_UMBRAL_PCT;
    while (nivel < GAS_NIVEL_MAX && pct >= UMBRAL[nivel + 1] + GAS_HISTERESIS_PCT) {
        nivel++;
    }
    while (nivel > 0 && pct < UMBRAL[nivel] - GAS_HISTERESIS_PCT) {
        nivel--;
    }
    return nivel;
}

void entradas_actualizar(EstadoECU *e)
{
    leer_digitales();

    e->ign_on         = s_estable[IN_IGNICION];
    e->sentido_cambio = s_estable[IN_SENTIDO];
    e->dir_izq        = s_estable[IN_DIR_IZQ];
    e->dir_der        = s_estable[IN_DIR_DER];
    e->intermitentes  = s_estable[IN_INTERMITENTES];
    e->freno_del      = s_estable[IN_FRENO_DEL];
    e->freno_tra      = s_estable[IN_FRENO_TRA];

    e->btn_arranque = flanco(IN_ARRANQUE);
    e->btn_cambio   = flanco(IN_CAMBIO);
    e->btn_reset    = flanco(IN_RESET);

    const bool primera_lectura = !s_adc_listo;
    e->adc_gasolina = leer_adc_promedio();
    const int pct = adc_a_porcentaje(e->adc_gasolina);
    /* En la primera lectura se parte de 0 para que el nivel suba directo al real */
    e->nivel_gasolina = calcular_nivel(primera_lectura ? 0 : e->nivel_gasolina, pct);
}

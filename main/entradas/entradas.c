#include "entradas.h"

#include "config.h"
#include "driver/gpio.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_check.h"
#include "esp_log.h"
#include "pines.h"

static const char *TAG = "entradas";

/* [AVANCE] Solo 4 entradas digitales están conectadas. Direccionales,
 * intermitentes y reset maestro quedan en falso; el cambio de marcha y su
 * sentido vienen del eje Y del joystick (ver leer_joystick). */
typedef enum {
    IN_IGNICION = 0,
    IN_ARRANQUE,
    IN_FRENO_DEL,
    IN_FRENO_TRA,
    IN_TOTAL
} entrada_t;

typedef struct {
    gpio_num_t pin;
    bool pull_up_interna;       /* false en 34..39: no tienen pull-up interna */
} entrada_def_t;

static const entrada_def_t DEF[IN_TOTAL] = {
    [IN_IGNICION]  = { PIN_SW_IGNICION,   false },  /* botón o cable a GND, 10 kΩ externa a 3.3 V */
    [IN_ARRANQUE]  = { PIN_BTN_ARRANQUE,  true  },
    [IN_FRENO_DEL] = { PIN_BTN_FRENO_DEL, true  },
    /* El TCRT5000 trae su pull-up; la interna solo se agrega si el pin la tiene */
    [IN_FRENO_TRA] = { PIN_BTN_FRENO_TRA, PIN_BTN_FRENO_TRA < GPIO_NUM_34 },
};

/* Estado del antirrebote de cada entrada */
static bool s_lectura_previa[IN_TOTAL];
static bool s_estable[IN_TOTAL];
static bool s_estable_previo[IN_TOTAL];

/* ADC: potenciómetro y joystick en la misma unidad ADC1 */
static adc_oneshot_unit_handle_t s_adc;
static adc_channel_t s_canal_gas;
static adc_channel_t s_canal_joy;
static uint16_t s_muestras[ADC_PROMEDIO];
static uint32_t s_suma;
static uint8_t s_indice;
static bool s_adc_listo;

#if IGNICION_CON_BOTON
/* Ignición con botón: estado encendido/apagado que cambia con cada pulsación */
static bool s_ign_encendida;
#endif

/* Joystick: zona de la lectura y si ya regresó al centro */
typedef enum { JOY_INTERMEDIO = 0, JOY_CENTRO, JOY_ALTO, JOY_BAJO } joy_zona_t;
static joy_zona_t s_joy_zona_previa;
static bool s_joy_armado;       /* false hasta ver el joystick en el centro */

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

    /* ADC1 en modo de una lectura (oneshot), dos canales */
    adc_unit_t unidad_gas;
    adc_unit_t unidad_joy;
    ESP_RETURN_ON_ERROR(adc_oneshot_io_to_channel(PIN_POT_GASOLINA, &unidad_gas, &s_canal_gas),
                        TAG, "el pin del potenciómetro no tiene ADC");
    ESP_RETURN_ON_ERROR(adc_oneshot_io_to_channel(PIN_JOY_Y, &unidad_joy, &s_canal_joy),
                        TAG, "el pin del joystick no tiene ADC");
    ESP_RETURN_ON_FALSE(unidad_gas == unidad_joy, ESP_ERR_INVALID_ARG, TAG,
                        "potenciómetro y joystick deben estar en la misma unidad ADC");

    const adc_oneshot_unit_init_cfg_t ucfg = {
        .unit_id = unidad_gas,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    ESP_RETURN_ON_ERROR(adc_oneshot_new_unit(&ucfg, &s_adc), TAG, "unidad ADC");
    const adc_oneshot_chan_cfg_t ccfg = {
        .atten = ADC_ATTEN_DB_12,       /* rango de 0 a ~3.1 V */
        .bitwidth = ADC_BITWIDTH_12,    /* 0..4095 */
    };
    ESP_RETURN_ON_ERROR(adc_oneshot_config_channel(s_adc, s_canal_gas, &ccfg), TAG, "canal gasolina");
    ESP_RETURN_ON_ERROR(adc_oneshot_config_channel(s_adc, s_canal_joy, &ccfg), TAG, "canal joystick");

    int joy = 0;
    adc_oneshot_read(s_adc, s_canal_joy, &joy);
    ESP_LOGI(TAG, "4 entradas digitales, potenciómetro (ADC%d canal %d) y joystick (canal %d)",
             unidad_gas + 1, s_canal_gas, s_canal_joy);
    ESP_LOGI(TAG, "joystick en reposo: %d (el centro debe quedar entre %d y %d)",
             joy, JOY_CENTRO_MIN, JOY_CENTRO_MAX);
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

static joy_zona_t zona_joystick(int crudo)
{
    if (crudo >= JOY_EXTREMO_ALTO) {
        return JOY_ALTO;
    }
    if (crudo <= JOY_EXTREMO_BAJO) {
        return JOY_BAJO;
    }
    if (crudo >= JOY_CENTRO_MIN && crudo <= JOY_CENTRO_MAX) {
        return JOY_CENTRO;
    }
    return JOY_INTERMEDIO;      /* entre el centro y un extremo: no cambia nada */
}

/* [AVANCE] El eje Y sustituye al botón de cambio y al switch de sentido.
 * Equivale a un botón con antirrebote: el extremo debe leerse dos ciclos
 * seguidos (20 ms) y el joystick debe volver al centro antes del siguiente
 * cambio, así que mantenerlo empujado da UN solo cambio. */
static void leer_joystick(EstadoECU *e)
{
    e->btn_cambio = false;

    int crudo = 0;
    if (adc_oneshot_read(s_adc, s_canal_joy, &crudo) != ESP_OK) {
        return;
    }
    const joy_zona_t zona = zona_joystick(crudo);

    if (zona == JOY_CENTRO) {
        s_joy_armado = true;
    } else if ((zona == JOY_ALTO || zona == JOY_BAJO) && zona == s_joy_zona_previa && s_joy_armado) {
        s_joy_armado = false;
        e->btn_cambio = true;
        e->sentido_cambio = (zona == JOY_ALTO) == (JOY_ALTO_SUBE != 0);
        ESP_LOGI(TAG, "joystick: %s marcha (adc %d)", e->sentido_cambio ? "subir" : "bajar", crudo);
    }
    s_joy_zona_previa = zona;
}

static uint16_t leer_adc_promedio(void)
{
    int crudo = 0;
    if (adc_oneshot_read(s_adc, s_canal_gas, &crudo) != ESP_OK) {
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

#if IGNICION_CON_BOTON
    /* Cada pulsación (flanco con antirrebote) invierte la ignición, como un
     * flip-flop T. Al arrancar la placa la ignición queda apagada. */
    if (flanco(IN_IGNICION)) {
        s_ign_encendida = !s_ign_encendida;
        ESP_LOGI(TAG, "botón de ignición: %s", s_ign_encendida ? "ENCENDIDA" : "APAGADA");
    }
    e->ign_on = s_ign_encendida;
#else
    e->ign_on = s_estable[IN_IGNICION];
#endif
    e->freno_del = s_estable[IN_FRENO_DEL];
    e->freno_tra = s_estable[IN_FRENO_TRA];

    /* No conectados en este avance */
    e->dir_izq       = false;
    e->dir_der       = false;
    e->intermitentes = false;
    e->btn_reset     = false;

    e->btn_arranque = flanco(IN_ARRANQUE);
    leer_joystick(e);           /* escribe btn_cambio y sentido_cambio */

    const bool primera_lectura = !s_adc_listo;
    e->adc_gasolina = leer_adc_promedio();
    const int pct = adc_a_porcentaje(e->adc_gasolina);
    /* En la primera lectura se parte de 0 para que el nivel suba directo al real */
    e->nivel_gasolina = calcular_nivel(primera_lectura ? 0 : e->nivel_gasolina, pct);
}

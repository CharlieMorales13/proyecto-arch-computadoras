#include "salidas_leds.h"

#include "config.h"
#include "driver/gpio.h"
#include "esp_check.h"
#include "pines.h"

static const char *TAG = "salidas_leds";

#define RGB_ROJO    LED_RGB_R
#define RGB_VERDE   LED_RGB_G
#define RGB_AZUL    LED_RGB_B
#define RGB_BLANCO  (LED_RGB_R | LED_RGB_G | LED_RGB_B)

static uint16_t s_bits_enviados;

/* [AVANCE] Sin 74HC595: cada bit de bits_leds va a su propio GPIO.
 * El orden del arreglo es el de los bits (LED_GAS_1 = bit 0 ... LED_RGB_B = bit 6).
 * Las direccionales (bits 7 y 8) se siguen calculando pero no tienen LED. */
static const gpio_num_t PINES[LED_SALIDAS_USADAS] = {
    PIN_LED_GAS_1, PIN_LED_GAS_2, PIN_LED_GAS_3, PIN_LED_GAS_4,
    PIN_LED_ESTADO_R, PIN_LED_ESTADO_G, PIN_LED_ESTADO_B,
};

static void escribir_leds(uint16_t bits)
{
    for (int i = 0; i < LED_SALIDAS_USADAS; i++) {
        gpio_set_level(PINES[i], (bits >> i) & 1u);
    }
}

esp_err_t salidas_leds_iniciar(void)
{
    uint64_t mascara = 1ULL << PIN_LED_VIDA;
    for (int i = 0; i < LED_SALIDAS_USADAS; i++) {
        mascara |= 1ULL << PINES[i];
    }
    const gpio_config_t cfg = {
        .pin_bit_mask = mascara,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&cfg), TAG, "gpio");
    escribir_leds(0);
    s_bits_enviados = 0;
    return ESP_OK;
}

static uint16_t barra_gasolina(const EstadoECU *e)
{
    switch (e->nivel_gasolina) {
    case 0:  return LED_GAS_1;                                  /* vacío: rojo fijo */
    case 1:  return e->fase_2hz ? LED_GAS_1 : 0;                /* reserva: 2 Hz */
    case 2:  return LED_GAS_1 | LED_GAS_2;
    case 3:  return LED_GAS_1 | LED_GAS_2 | LED_GAS_3;
    default: return LED_GAS_1 | LED_GAS_2 | LED_GAS_3 | LED_GAS_4;
    }
}

static uint16_t direccionales(const EstadoECU *e)
{
    if (!e->fase_1hz) {
        return 0;
    }
    if (e->intermitentes) {
        return LED_DIR_IZQ | LED_DIR_DER;
    }
    return (e->dir_izq ? LED_DIR_IZQ : 0) | (e->dir_der ? LED_DIR_DER : 0);
}

static uint16_t color_rgb(const EstadoECU *e)
{
    switch (e->estado) {
    case ESTADO_APAGADO:
        return 0;
    case ESTADO_FRENADO:
        if (e->freno_del && e->freno_tra) {
            return e->fase_5hz ? RGB_ROJO : 0;      /* ABS */
        }
        return RGB_ROJO;
    case ESTADO_BLOQUEADO:
    case ESTADO_SIN_COMBUSTIBLE:
        return RGB_ROJO;
    case ESTADO_MODO_PRUEBAS:
        return RGB_BLANCO;
    default:
        break;
    }
    if (e->flag_servicio) {
        return RGB_AZUL;
    }
    return (e->estado == ESTADO_NEUTRAL) ? RGB_BLANCO : RGB_VERDE;
}

void salidas_leds_actualizar(EstadoECU *e)
{
    gpio_set_level(PIN_LED_VIDA, e->fase_1hz);     /* el ciclo sigue vivo */

    uint16_t bits = 0;
    if (e->estado == ESTADO_MODO_PRUEBAS && e->rutina == RUTINA_LEDS) {
        /* Rutina 1: barrido de las salidas con LED, 200 ms cada una */
        bits = (uint16_t)(1u << ((e->ciclos_en_estado / 20u) % LED_SALIDAS_USADAS));
    } else if (e->estado != ESTADO_APAGADO) {
        bits = barra_gasolina(e) | direccionales(e) | color_rgb(e);
    }

    e->bits_leds = bits;
    if (bits != s_bits_enviados) {
        escribir_leds(bits);
        s_bits_enviados = bits;
    }
}

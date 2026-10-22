#include "servos.h"

#include "config.h"
#include "driver/gpio.h"
#include "esp_check.h"
#include "esp_log.h"
#include "pines.h"

static const char *TAG = "motor";

/* [AVANCE] En lugar de dos servos por LEDC, un relé enciende el motor DC.
 * El cálculo de la velocidad (rampa de subida y frenado) es el mismo de la
 * versión con servos: km_rpm y fsm_ecu siguen leyendo e->velocidad. Lo único
 * que cambia es la etapa final: antes velocidad -> ancho de pulso; ahora
 * velocidad > 0 -> relé cerrado. */

static bool s_motor_encendido;
static uint32_t s_ciclo_conmutacion;

static void rele_escribir(bool encender)
{
#if RELE_ACTIVO_EN_BAJO
    /* Salida de drenador abierto: 0 = jala a GND (relé activo),
     * 1 = alta impedancia (relé suelto). Así los 3.3 V del ESP32 no dejan el
     * módulo de 5 V a medio activar. */
    gpio_set_level(PIN_RELE_MOTOR, encender ? 0 : 1);
#else
    gpio_set_level(PIN_RELE_MOTOR, encender ? 1 : 0);
#endif
}

esp_err_t servos_iniciar(void)
{
    /* Primero el nivel de "apagado" y después se habilita la salida, para
     * que el relé no dé un clic al arrancar. */
    rele_escribir(false);
    const gpio_config_t cfg = {
        .pin_bit_mask = 1ULL << PIN_RELE_MOTOR,
#if RELE_ACTIVO_EN_BAJO
        .mode = GPIO_MODE_OUTPUT_OD,
#else
        .mode = GPIO_MODE_OUTPUT,
#endif
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&cfg), TAG, "gpio del relé");
    rele_escribir(false);
    s_motor_encendido = false;
    s_ciclo_conmutacion = 0;
    ESP_LOGI(TAG, "relé del motor en GPIO %d (activo en %s)", PIN_RELE_MOTOR,
             RELE_ACTIVO_EN_BAJO ? "bajo" : "alto");
    return ESP_OK;
}

static int16_t velocidad_objetivo(const EstadoECU *e)
{
    switch (e->estado) {
    case ESTADO_ARRANQUE:
    case ESTADO_EN_MARCHA:
        return (int16_t)(e->marcha * VEL_POR_MARCHA);
    case ESTADO_MODO_PRUEBAS:
        if (e->rutina == RUTINA_SERVOS) {
            /* Recorre las marchas 1..4, 2 s en cada una */
            const uint32_t marcha = 1 + (e->ciclos_en_estado / PRUEBA_MOTOR_CICLOS) % MARCHA_MAX;
            return (int16_t)(marcha * VEL_POR_MARCHA);
        }
        return 0;           /* motor inhibido en las demás rutinas */
    default:
        return 0;
    }
}

static int16_t paso_bajada(const EstadoECU *e)
{
    if (e->estado == ESTADO_FRENADO) {
        if (e->freno_del && e->freno_tra) {
            return FRENO_ABS_POR_CICLO;
        }
        if (e->freno_del) {
            return FRENO_DEL_POR_CICLO;
        }
        if (e->freno_tra) {
            return FRENO_TRA_POR_CICLO;
        }
    }
    if (e->estado == ESTADO_MODO_PRUEBAS) {
        return VEL_MAX;     /* al cambiar de rutina se detiene de inmediato */
    }
    return DESACEL_MOTOR_POR_CICLO;
}

static void actualizar_rele(const EstadoECU *e, bool encender)
{
    if (encender == s_motor_encendido) {
        return;
    }
    /* Apagar es inmediato (seguridad). Encender respeta un tiempo mínimo desde
     * la última conmutación para que el relé no "tartamudee". */
    if (encender && e->ciclos - s_ciclo_conmutacion < RELE_CICLOS_MIN) {
        return;
    }
    rele_escribir(encender);
    s_motor_encendido = encender;
    s_ciclo_conmutacion = e->ciclos;
    ESP_LOGI(TAG, "motor %s", encender ? "ENCENDIDO" : "APAGADO");
}

void servos_actualizar(EstadoECU *e)
{
    if (e->estado == ESTADO_APAGADO) {
        e->velocidad = 0;
        actualizar_rele(e, false);
        return;
    }

    const int16_t objetivo = velocidad_objetivo(e);
    if (e->velocidad < objetivo) {
        e->velocidad += ACEL_POR_CICLO;
        if (e->velocidad > objetivo) {
            e->velocidad = objetivo;
        }
    } else if (e->velocidad > objetivo) {
        e->velocidad -= paso_bajada(e);
        if (e->velocidad < objetivo) {
            e->velocidad = objetivo;
        }
    }

    actualizar_rele(e, e->velocidad > 0);
}

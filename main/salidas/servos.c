#include "servos.h"

#include "config.h"
#include "driver/ledc.h"
#include "esp_check.h"
#include "pines.h"

static const char *TAG = "servos";

#define SERVO_MODO   LEDC_LOW_SPEED_MODE
#define SERVO_TIMER  LEDC_TIMER_0
#define CANAL_IZQ    LEDC_CHANNEL_0
#define CANAL_DER    LEDC_CHANNEL_1

/* Último duty escrito en cada canal y ciclo en que se escribió.
 * servos_iniciar() deja ambos canales en duty 0 (ciclo 0). */
static uint32_t s_duty_izq;
static uint32_t s_duty_der;
static uint32_t s_ciclo_escritura;

static uint32_t us_a_duty(uint32_t us)
{
    return us * (1u << SERVO_BITS) / SERVO_PERIODO_US;
}

/* En el ESP32, ledc_update_duty() espera activamente (dentro de una sección
 * crítica) a que el hardware aplique el duty anterior, y eso ocurre hasta el
 * inicio del siguiente periodo del PWM: hasta 20 ms a 50 Hz por canal.
 * Escribir los dos canales en cada ciclo de 10 ms costaba ~40 ms por ciclo.
 * Por eso solo se escribe cuando el pulso cambia y han pasado al menos
 * SERVO_CICLOS_ENTRE_ESCRITURAS ciclos (30 ms): para entonces ya pasó un
 * periodo completo y la llamada no espera. */
static void escribir_pulsos(const EstadoECU *e, uint32_t us_izq, uint32_t us_der)
{
    const uint32_t duty_izq = us_a_duty(us_izq);
    const uint32_t duty_der = us_a_duty(us_der);
    if (duty_izq == s_duty_izq && duty_der == s_duty_der) {
        return;
    }
    if (e->ciclos - s_ciclo_escritura < SERVO_CICLOS_ENTRE_ESCRITURAS) {
        return;     /* se escribe en un ciclo siguiente, con el valor de ese momento */
    }
    ledc_set_duty(SERVO_MODO, CANAL_IZQ, duty_izq);
    ledc_update_duty(SERVO_MODO, CANAL_IZQ);
    ledc_set_duty(SERVO_MODO, CANAL_DER, duty_der);
    ledc_update_duty(SERVO_MODO, CANAL_DER);
    s_duty_izq = duty_izq;
    s_duty_der = duty_der;
    s_ciclo_escritura = e->ciclos;
}

esp_err_t servos_iniciar(void)
{
    const ledc_timer_config_t timer = {
        .speed_mode = SERVO_MODO,
        .duty_resolution = (ledc_timer_bit_t)SERVO_BITS,
        .timer_num = SERVO_TIMER,
        .freq_hz = SERVO_FREQ_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_RETURN_ON_ERROR(ledc_timer_config(&timer), TAG, "timer LEDC");

    const gpio_num_t pines[2] = { PIN_SERVO_IZQ, PIN_SERVO_DER };
    const ledc_channel_t canales[2] = { CANAL_IZQ, CANAL_DER };
    for (int i = 0; i < 2; i++) {
        const ledc_channel_config_t canal = {
            .gpio_num = pines[i],
            .speed_mode = SERVO_MODO,
            .channel = canales[i],
            .timer_sel = SERVO_TIMER,
            .duty = 0,          /* sin pulsos hasta el primer ciclo */
            .hpoint = 0,
        };
        ESP_RETURN_ON_ERROR(ledc_channel_config(&canal), TAG, "canal %d", i);
    }
    s_duty_izq = 0;
    s_duty_der = 0;
    s_ciclo_escritura = 0;
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
            const uint32_t marcha = 1 + (e->ciclos_en_estado / PRUEBA_SERVOS_CICLOS) % MARCHA_MAX;
            return (int16_t)(marcha * VEL_POR_MARCHA);
        }
        return 0;           /* servos inhibidos en las demás rutinas */
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
        return VEL_MAX;     /* al cambiar de rutina se detienen de inmediato */
    }
    return DESACEL_MOTOR_POR_CICLO;
}

void servos_actualizar(EstadoECU *e)
{
    if (e->estado == ESTADO_APAGADO) {
        e->velocidad = 0;
        escribir_pulsos(e, 0, 0);   /* sin pulsos: detenidos aunque el neutro no esté calibrado */
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

    const int32_t delta_us = (int32_t)e->velocidad * SERVO_RANGO_US / VEL_MAX;
    escribir_pulsos(e, (uint32_t)(SERVO_NEUTRO_IZQ_US + delta_us),
                    (uint32_t)(SERVO_NEUTRO_DER_US - delta_us));
}

#include "km_rpm.h"

#include "config.h"
#include "esp_log.h"

static const char *TAG = "km_rpm";

/* Fracción de vuelta acumulada, en millonésimas de vuelta */
static uint32_t s_microvueltas;
/* Kilometraje del ciclo anterior, para detectar el cruce de cada 16 km */
static uint32_t s_km_previo;
static bool s_km_previo_listo;

static uint16_t rpm_objetivo(const EstadoECU *e)
{
    static const uint16_t RPM[MARCHA_MAX + 1] = RPM_MARCHA;
    switch (e->estado) {
    case ESTADO_ARRANQUE:
    case ESTADO_FRENADO:
        return RPM_RALENTI;
    case ESTADO_EN_MARCHA:
        return RPM[e->marcha];
    default:
        return 0;
    }
}

static void actualizar_rpm(EstadoECU *e)
{
    const uint16_t objetivo = rpm_objetivo(e);
    if (e->estado == ESTADO_APAGADO) {
        e->rpm = 0;
    } else if (e->rpm + RPM_RAMPA_POR_CICLO <= objetivo) {
        e->rpm += RPM_RAMPA_POR_CICLO;
    } else if (e->rpm >= objetivo + RPM_RAMPA_POR_CICLO) {
        e->rpm -= RPM_RAMPA_POR_CICLO;
    } else {
        e->rpm = objetivo;
    }
}

static void actualizar_km(EstadoECU *e)
{
    if (e->velocidad <= 0) {
        return;
    }
    /* vueltas por ciclo = (vel/10000) * (rps_max/100) * 0.01 s
     * en millonésimas: vel * rps_max_x100 / 100 */
    s_microvueltas += (uint32_t)e->velocidad * RUEDA_RPS_MAX_X100 / 100u;
    while (s_microvueltas >= 1000000u) {
        s_microvueltas -= 1000000u;
        e->km_m += METROS_POR_VUELTA;
    }
}

static void actualizar_servicio(EstadoECU *e)
{
    /* En el primer ciclo se toma el km cargado de la NVS como referencia */
    if (!s_km_previo_listo) {
        s_km_previo = e->km_m;
        s_km_previo_listo = true;
    }

    if (e->km_m / SERVICIO_CADA_METROS > s_km_previo / SERVICIO_CADA_METROS) {
        e->flag_servicio = true;
        ESP_LOGW(TAG, "servicio requerido: %lu m recorridos", (unsigned long)e->km_m);
    }
    s_km_previo = e->km_m;

    /* Petición de fsm_diag (rutina 4): km_rpm es el único que escribe la bandera */
    if (e->quitar_servicio && e->flag_servicio) {
        e->flag_servicio = false;
        ESP_LOGI(TAG, "aviso de servicio desactivado (rutina 4); km conservados");
    }
}

void km_rpm_actualizar(EstadoECU *e)
{
    actualizar_rpm(e);
    actualizar_km(e);
    actualizar_servicio(e);
}

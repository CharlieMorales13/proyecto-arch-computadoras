#include "fsm_ecu.h"

#include "config.h"
#include "esp_log.h"

static const char *TAG = "fsm_ecu";

static bool en_movimiento(estado_ecu_t s)
{
    return s == ESTADO_ARRANQUE || s == ESTADO_EN_MARCHA || s == ESTADO_FRENADO;
}

static bool tecla_es(const EstadoECU *e, char t)
{
    return e->tecla_valida && e->tecla == t;
}

static bool tecla_de_rutina(const EstadoECU *e)
{
    return e->tecla_valida && e->tecla >= '1' && e->tecla <= '4';
}

/* Función de transición: devuelve el siguiente estado sin modificar nada.
 * El orden de los if es el arbitraje entre eventos simultáneos. */
static estado_ecu_t siguiente_estado(const EstadoECU *e)
{
    const bool freno = e->freno_del || e->freno_tra;
    const bool salir = tecla_es(e, 'D') || tecla_es(e, '*');

    /* Prioridad 1 */
    if (!e->ign_on) {
        return ESTADO_APAGADO;
    }
    /* Prioridad 2 */
    if (e->nivel_gasolina == GAS_NIVEL_VACIO) {
        return ESTADO_SIN_COMBUSTIBLE;
    }
    /* Prioridad 3 */
    if (freno && en_movimiento(e->estado)) {
        return ESTADO_FRENADO;
    }

    switch (e->estado) {
    case ESTADO_APAGADO:
    case ESTADO_SIN_COMBUSTIBLE:
        return ESTADO_NEUTRAL;      /* ignición activa y gasolina disponible */

    /* Prioridad 4: modo de pruebas (solo se entra desde NEUTRAL) */
    case ESTADO_BLOQUEADO:
        if (e->sys_unlocked) {
            return ESTADO_MODO_PRUEBAS;
        }
        return salir ? ESTADO_NEUTRAL : ESTADO_BLOQUEADO;

    case ESTADO_MODO_PRUEBAS:
        return tecla_es(e, 'D') ? ESTADO_NEUTRAL : ESTADO_MODO_PRUEBAS;

    case ESTADO_NEUTRAL:
        /* Una tecla (menos * y D) pide el modo de pruebas */
        if (e->tecla_valida && !salir) {
            if (!e->sys_unlocked) {
                return ESTADO_BLOQUEADO;
            }
            if (tecla_de_rutina(e)) {
                return ESTADO_MODO_PRUEBAS;
            }
        }
        /* Prioridad 5: operación normal */
        return e->btn_arranque ? ESTADO_ARRANQUE : ESTADO_NEUTRAL;

    case ESTADO_ARRANQUE:
        return (e->ciclos_en_estado >= ARRANQUE_CICLOS) ? ESTADO_EN_MARCHA : ESTADO_ARRANQUE;

    case ESTADO_EN_MARCHA:
        return ESTADO_EN_MARCHA;

    case ESTADO_FRENADO:
        /* Se soltaron los frenos: sigue si aún hay velocidad, si no queda en neutral */
        return (e->velocidad > 0) ? ESTADO_EN_MARCHA : ESTADO_NEUTRAL;

    default:
        return ESTADO_APAGADO;
    }
}

static void actualizar_marcha(EstadoECU *e)
{
    if (!e->btn_cambio || e->estado == ESTADO_APAGADO || e->estado == ESTADO_MODO_PRUEBAS) {
        return;
    }
    if (e->sentido_cambio && e->marcha < MARCHA_MAX) {
        e->marcha++;
    } else if (!e->sentido_cambio && e->marcha > MARCHA_MIN) {
        e->marcha--;
    }
}

void fsm_ecu_actualizar(EstadoECU *e)
{
    /* La marcha cargada de NVS podría venir fuera de rango si la memoria se corrompe */
    if (e->marcha < MARCHA_MIN || e->marcha > MARCHA_MAX) {
        e->marcha = MARCHA_MIN;
    }

    const estado_ecu_t siguiente = siguiente_estado(e);
    if (siguiente != e->estado) {
        ESP_LOGI(TAG, "%s -> %s", estado_nombre(e->estado), estado_nombre(siguiente));
        e->estado = siguiente;
        e->ciclos_en_estado = 0;
    } else {
        e->ciclos_en_estado++;
    }

    actualizar_marcha(e);
}

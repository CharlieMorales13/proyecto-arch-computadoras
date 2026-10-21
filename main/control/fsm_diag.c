#include "fsm_diag.h"

#include <string.h>

#include "config.h"
#include "esp_log.h"

static const char *TAG = "fsm_diag";

static char s_escrito[CLAVE_LONGITUD + 1];

static void borrar(EstadoECU *e)
{
    memset(s_escrito, 0, sizeof(s_escrito));
    e->clave_digitos = 0;
}

static void actualizar_clave(EstadoECU *e)
{
    if (e->sys_unlocked || !e->tecla_valida) {
        return;
    }
    if (e->estado != ESTADO_NEUTRAL && e->estado != ESTADO_BLOQUEADO) {
        return;
    }

    const char t = e->tecla;
    if (t >= '0' && t <= '9') {
        if (e->clave_digitos < CLAVE_LONGITUD) {
            s_escrito[e->clave_digitos++] = t;
        }
    } else if (t == '#') {
        if (strcmp(s_escrito, CLAVE_PRUEBAS) == 0) {
            e->sys_unlocked = true;
            ESP_LOGI(TAG, "clave correcta: modo de pruebas desbloqueado");
        } else {
            ESP_LOGW(TAG, "clave incorrecta");
        }
        borrar(e);
    } else if (t == '*' || t == 'D') {
        borrar(e);
    }
}

static void actualizar_rutina(EstadoECU *e)
{
    if (e->estado != ESTADO_MODO_PRUEBAS) {
        e->rutina = RUTINA_MENU;
    }
    const bool tecla_rutina = e->tecla_valida && e->tecla >= '1' && e->tecla <= '4';
    const bool acepta = e->estado == ESTADO_MODO_PRUEBAS || e->estado == ESTADO_NEUTRAL;
    if (e->sys_unlocked && tecla_rutina && acepta) {
        e->rutina = (uint8_t)(e->tecla - '0');
        /* La rutina 4 no apaga la bandera de servicio: solo la pide.
         * flag_servicio tiene un único escritor, km_rpm. */
        e->quitar_servicio = (e->rutina == RUTINA_SERVICIO);
    }
}

void fsm_diag_actualizar(EstadoECU *e)
{
    e->quitar_servicio = false;     /* la petición dura un solo ciclo */

    /* Apagar la ignición vuelve a bloquear el modo de pruebas */
    if (e->estado == ESTADO_APAGADO) {
        e->sys_unlocked = false;
        e->rutina = RUTINA_MENU;
        borrar(e);
        return;
    }
    actualizar_clave(e);
    actualizar_rutina(e);
}

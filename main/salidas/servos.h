/*
 * servos.h — Tracción (equivale a ctrl_pwm_servos en VHDL).
 *
 * [AVANCE] El módulo conserva su nombre y su interfaz, pero en esta rama
 * maneja un motor DC con reductor a través de un relé en lugar de dos
 * servos. main.c no cambia.
 *
 * La velocidad (0..10000) sube a 50 %/s y baja según el freno: delantero
 * -75 %/s, trasero -25 %/s, ambos (ABS) -100 %/s; sin freno -30 %/s.
 * Igual que con los servos, km_rpm usa esa velocidad para el kilometraje y
 * fsm_ecu para saber si FRENADO regresa a EN_MARCHA o a NEUTRAL.
 *
 * Salida: relé cerrado mientras velocidad > 0. El relé solo da encendido y
 * apagado, así que la marcha se ve en las RPM y en la LCD, no en la llanta
 * (limitación conocida; con un driver L298N/L9110S y PWM se vuelve
 * proporcional). En APAGADO el relé se abre de inmediato.
 *
 * Escribe: velocidad.
 */
#pragma once

#include "esp_err.h"
#include "estado.h"

esp_err_t servos_iniciar(void);
void servos_actualizar(EstadoECU *e);

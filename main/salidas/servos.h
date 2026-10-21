/*
 * servos.h — Dos PWM de 50 Hz con el periférico LEDC (14 bits)
 * (equivale a ctrl_pwm_servos en VHDL).
 *
 * Los servos son de giro continuo: el ancho de pulso fija la VELOCIDAD,
 * no el ángulo. 1500 µs = quieto; marcha 1..4 = ±100, ±200, ±300, ±400 µs.
 * El servo derecho recibe el pulso reflejado (gira al revés) porque va
 * montado en espejo.
 *
 *   duty = t / 20000 µs * 16384      (1500 µs -> 1229, 1900 -> 1556, 1100 -> 901)
 *
 * La velocidad (0..10000) sube a 50 %/s y baja según el freno: delantero
 * -75 %/s, trasero -25 %/s, ambos (ABS) -100 %/s; sin freno -30 %/s.
 * En APAGADO se cortan los pulsos.
 *
 * La velocidad se calcula cada 10 ms, pero el PWM solo se escribe cuando el
 * pulso cambia y con al menos 30 ms entre escrituras: en el ESP32,
 * ledc_update_duty() espera hasta el siguiente periodo de 20 ms si el duty
 * anterior aún no se aplicó.
 *
 * Escribe: velocidad.
 */
#pragma once

#include "esp_err.h"
#include "estado.h"

esp_err_t servos_iniciar(void);
void servos_actualizar(EstadoECU *e);

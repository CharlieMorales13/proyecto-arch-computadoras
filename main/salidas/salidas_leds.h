/*
 * salidas_leds.h — Barra de gasolina y LED de estado (equivale a ctrl_rgb +
 * ctrl_leds en VHDL).
 *
 * [AVANCE] Sin 74HC595: los 7 LEDs van directo a GPIO (ver pines.h). La
 * lógica que arma bits_leds no cambia; solo la función que los escribe.
 *
 *   bit 0..3  barra de gasolina (rojo, amarillo, verde, verde)
 *   bit 4..6  LED de estado R, G, B (3 LEDs separados o un RGB de cátodo común)
 *   bit 7, 8  direccionales (se calculan, pero no hay LED en este avance)
 *
 * Gasolina: nivel 4..2 = 4..2 LEDs; nivel 1 (reserva) = rojo a 2 Hz;
 *           nivel 0 (vacío) = rojo fijo.
 * Estado, prioridad rojo > azul > blanco > verde:
 *   rojo   FRENADO (a 5 Hz si ambos frenos = ABS), BLOQUEADO, SIN_COMBUSTIBLE
 *   azul   flag_servicio
 *   blanco NEUTRAL (con 3 LEDs separados: los tres encendidos)
 *   verde  ARRANQUE y EN_MARCHA
 * También hace parpadear el LED de vida (GPIO 2) a 1 Hz.
 *
 * Escribe: bits_leds.
 */
#pragma once

#include "esp_err.h"
#include "estado.h"

#define LED_GAS_1     (1u << 0)     /* rojo */
#define LED_GAS_2     (1u << 1)     /* amarillo */
#define LED_GAS_3     (1u << 2)     /* verde */
#define LED_GAS_4     (1u << 3)     /* verde */
#define LED_RGB_R     (1u << 4)
#define LED_RGB_G     (1u << 5)
#define LED_RGB_B     (1u << 6)
#define LED_DIR_IZQ   (1u << 7)
#define LED_DIR_DER   (1u << 8)
#define LED_SALIDAS_USADAS  7   /* bits con LED físico (0..6) */

esp_err_t salidas_leds_iniciar(void);
void salidas_leds_actualizar(EstadoECU *e);

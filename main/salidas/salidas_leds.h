/*
 * salidas_leds.h — Barra de gasolina, LED RGB y direccionales en los dos
 * 74HC595 en cadena (equivale a ctrl_rgb + ctrl_leds en VHDL).
 * Es el único módulo que escribe en los registros de corrimiento.
 *
 *   Q0..Q3  barra de gasolina (Q0 rojo, Q1 amarillo, Q2 y Q3 verdes)
 *   Q4..Q6  LED RGB (R, G, B)
 *   Q7, Q8  direccional izquierda y derecha (Q8 = primera salida del 2.º chip)
 *
 * Gasolina: nivel 4..2 = 4..2 LEDs; nivel 1 (reserva) = rojo a 2 Hz;
 *           nivel 0 (vacío) = rojo fijo.
 * RGB, prioridad rojo > azul > blanco > verde:
 *   rojo   FRENADO (a 5 Hz si ambos frenos = ABS), BLOQUEADO, SIN_COMBUSTIBLE
 *   azul   flag_servicio
 *   blanco NEUTRAL
 *   verde  ARRANQUE y EN_MARCHA
 * Direccionales e intermitentes a 1 Hz; intermitentes tienen prioridad.
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
#define LED_SALIDAS_USADAS  9

esp_err_t salidas_leds_iniciar(void);
void salidas_leds_actualizar(EstadoECU *e);

/*
 * display.h — Display de 8 dígitos con MAX7219 por SPI (equivale a mux_7seg
 * en VHDL). El chip hace el multiplexado; el ESP32 solo envía los dígitos
 * cada 100 ms y únicamente si cambiaron.
 *
 *   ARRANQUE, EN_MARCHA, FRENADO -> RPM y marcha      "9000   4"
 *   NEUTRAL, SIN_COMBUSTIBLE     -> km y "S" si hay servicio  "S    12.3"
 *   BLOQUEADO                    -> un guion por dígito tecleado "PIn --"
 *   MODO_PRUEBAS                 -> "PruEbA n"; rutina 2 = "8.8.8.8.8.8.8.8."
 *   APAGADO                      -> apagado
 *
 * Escribe: digitos.
 */
#pragma once

#include "esp_err.h"
#include "estado.h"

esp_err_t display_iniciar(void);
void display_actualizar(EstadoECU *e);

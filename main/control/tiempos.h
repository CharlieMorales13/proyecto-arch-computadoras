/*
 * tiempos.h — Divisor de frecuencia (equivale a divisor_frecuencia en VHDL).
 *
 * Cuenta ciclos de 10 ms y genera:
 *   - marcas de un ciclo: tick_100ms, tick_250ms, tick_500ms, tick_1s
 *   - ondas cuadradas para parpadeo: fase_1hz, fase_2hz, fase_5hz
 * Escribe: ciclos, tick_*, fase_*.
 */
#pragma once

#include "estado.h"

void tiempos_actualizar(EstadoECU *e);

/*
 * fsm_ecu.h — Unidad de control: máquina de 8 estados
 * (equivale a fsm_principal_ecu en VHDL).
 *
 * Prioridad de evaluación en cada ciclo:
 *   1. ignición apagada      -> APAGADO
 *   2. sin combustible       -> SIN_COMBUSTIBLE
 *   3. freno con la moto en movimiento -> FRENADO
 *   4. modo de pruebas (teclas en NEUTRAL, BLOQUEADO y MODO_PRUEBAS)
 *   5. operación normal (arranque, marcha, salir de frenado)
 *
 * También lleva la marcha (1 a 4) con el botón de cambio y el interruptor
 * de sentido. La rutina de pruebas la elige fsm_diag.
 *
 * Escribe: estado, marcha, ciclos_en_estado.
 */
#pragma once

#include "estado.h"

void fsm_ecu_actualizar(EstadoECU *e);

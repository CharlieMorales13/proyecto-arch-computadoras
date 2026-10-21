/*
 * fsm_diag.h — Clave y rutina del modo de pruebas
 * (equivale a fsm_diagnostico en VHDL).
 *
 * Clave: acepta teclas en NEUTRAL o BLOQUEADO mientras el sistema siga
 * bloqueado:
 *   - dígitos  : se guardan (máximo 4)
 *   - '#'      : compara con 2026; si coincide, sys_unlocked = true;
 *                si no, borra lo escrito
 *   - '*' o 'D': borran lo escrito (fsm_ecu regresa a NEUTRAL)
 * El desbloqueo dura hasta que se apaga la ignición (no se guarda en NVS).
 *
 * Rutina: con el sistema desbloqueado, las teclas 1 a 4 en NEUTRAL o en
 * MODO_PRUEBAS eligen la rutina. Fuera de MODO_PRUEBAS vuelve a "menú".
 * Al elegir la rutina 4 levanta quitar_servicio durante un ciclo; quien
 * apaga flag_servicio es km_rpm (un solo escritor por campo).
 *
 * Escribe: sys_unlocked, clave_digitos, rutina, quitar_servicio.
 */
#pragma once

#include "estado.h"

void fsm_diag_actualizar(EstadoECU *e);

/*
 * teclado.h — Teclado matricial 4x4 a través del expansor PCF8574 por I2C
 * (equivale a driver_teclado en VHDL).
 *
 * Conexión: P0..P3 = filas 1..4, P4..P7 = columnas 1..4.
 *
 * Máquina de estados: en cada ciclo de 10 ms se barre UNA fila (fila en 0,
 * columnas como entrada) para no ocupar el bus I2C todo el ciclo. Un barrido
 * completo toma 4 ciclos (40 ms). Una tecla se entrega cuando aparece igual
 * en dos barridos completos seguidos, una sola vez por pulsación.
 *
 * Si una transferencia I2C falla, el teclado se pausa 1 s antes de reintentar,
 * para que un PCF8574 desconectado no alargue cada ciclo de 10 ms.
 *
 * Escribe: tecla, tecla_valida.
 */
#pragma once

#include "esp_err.h"
#include "estado.h"

/* Si el PCF8574 no responde devuelve ESP_ERR_NOT_FOUND y el teclado queda
 * deshabilitado (el resto de la ECU sigue funcionando). */
esp_err_t teclado_iniciar(void);
void teclado_actualizar(EstadoECU *e);

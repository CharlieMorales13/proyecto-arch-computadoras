/*
 * persistencia.h — Memoria no volátil (NVS en la flash del ESP32)
 * (equivale a banco_registros en VHDL). Es el único módulo que toca la NVS.
 *
 * - Al arrancar: carga marcha, km_m y flag_servicio en EstadoECU.
 * - En el flanco de apagado de la ignición: guarda los tres valores
 *   (solo los que cambiaron, para no desgastar la flash).
 * - Con btn_reset y la ignición apagada: borra la NVS y reinicia el ESP32,
 *   que vuelve a arrancar con los valores de fábrica. Con la ignición
 *   encendida el reset se ignora.
 *
 * Escribe: nada de EstadoECU durante el ciclo (solo lee). En el arranque
 * escribe los valores iniciales de marcha, km_m y flag_servicio.
 */
#pragma once

#include "esp_err.h"
#include "estado.h"

esp_err_t persistencia_iniciar(EstadoECU *e);
void persistencia_actualizar(EstadoECU *e);

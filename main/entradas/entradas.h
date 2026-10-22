/*
 * entradas.h — Lectura de los controles y de los ADC
 * (equivale a debounce_universal en VHDL).
 *
 * [AVANCE] Conectados: ignición, arranque (botón del joystick), freno
 * delantero (microinterruptor), freno trasero (TCRT5000), potenciómetro de
 * gasolina y eje Y del joystick (cambio de marcha). Direccionales,
 * intermitentes y reset maestro se reportan en falso.
 *
 * - Todas las entradas digitales son activas en bajo (0 V = presionado / encendido).
 * - Una entrada cambia solo tras 2 lecturas iguales seguidas (20 ms).
 * - Arranque y cambio generan un evento que dura un solo ciclo.
 * - El joystick da UN cambio por cada vez que se lleva a un extremo; hay que
 *   regresarlo al centro para el siguiente.
 * - El ADC de gasolina se promedia y se convierte a nivel 0..4 con histéresis.
 *
 * Escribe: ign_on, sentido_cambio, dir_izq, dir_der, intermitentes,
 *          freno_del, freno_tra, btn_arranque, btn_cambio, btn_reset,
 *          adc_gasolina, nivel_gasolina.
 */
#pragma once

#include "esp_err.h"
#include "estado.h"

esp_err_t entradas_iniciar(void);
void entradas_actualizar(EstadoECU *e);

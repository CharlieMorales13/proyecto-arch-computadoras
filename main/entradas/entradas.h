/*
 * entradas.h — Lectura de los 10 controles y del potenciómetro
 * (equivale a debounce_universal en VHDL).
 *
 * - Todas las entradas son activas en bajo (0 V = presionado / encendido).
 * - Una entrada cambia solo tras 2 lecturas iguales seguidas (20 ms).
 * - Los botones de arranque, cambio y reset generan un evento que dura un
 *   solo ciclo (flanco de presión).
 * - El ADC se promedia y se convierte a nivel de gasolina 0..4 con histéresis.
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

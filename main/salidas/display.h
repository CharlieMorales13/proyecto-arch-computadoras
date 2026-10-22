/*
 * display.h — LCD 16x2 con controlador HD44780 (equivale a mux_7seg en VHDL).
 *
 * [AVANCE] Sustituye al MAX7219 de 8 dígitos. Funciona con la LCD de módulo
 * I2C (PCF8574) o conectada en paralelo de 4 bits; se elige con LCD_I2C en
 * pines.h.
 *
 *   ARRANQUE, EN_MARCHA, FRENADO -> estado, marcha, RPM y km
 *        "EN MARCHA    M3"
 *        "5000rpm   12.3km"
 *   NEUTRAL, SIN_COMBUSTIBLE     -> estado, marcha, km y "SERV" si hay servicio
 *   BLOQUEADO                    -> un asterisco por dígito tecleado
 *   MODO_PRUEBAS                 -> rutina elegida; rutina 2 = todos los puntos
 *   APAGADO                      -> en blanco (con I2C también apaga la luz)
 *
 * El texto se recalcula cada 100 ms y en cada ciclo de 10 ms solo se envían
 * hasta 6 caracteres que hayan cambiado, para no alargar el ciclo. Cada 2 s
 * la LCD se re-sincroniza sin borrar la pantalla, por si el ruido del motor
 * la desfasó. Si la LCD no responde, se avisa y se reintenta cada segundo.
 *
 * Escribe: lcd.
 */
#pragma once

#include "esp_err.h"
#include "estado.h"

esp_err_t display_iniciar(void);
void display_actualizar(EstadoECU *e);

/*
 * pines.h — Asignación de GPIO (equivale al archivo de restricciones XDC).
 *
 * Mapa de pines del Segundo Avance (Anexo B) para el ESP32 DevKit V1 de
 * 38 pines. Es el único archivo con números de pin.
 *
 * No se usan GPIO 0, 1, 3, 6 a 11 ni 12 (arranque, flash y USB).
 * GPIO 34, 35, 36 y 39 son solo de entrada y NO tienen pull-up interna:
 * llevan resistencia externa de 10 kΩ a 3.3 V.
 */
#pragma once

#include "driver/gpio.h"

/* Servomotores (LEDC canales 0 y 1) */
#define PIN_SERVO_IZQ         GPIO_NUM_25
#define PIN_SERVO_DER         GPIO_NUM_26

/* Un solo MAX7219 de 8 dígitos por SPI3 (VSPI), a través del convertidor de
 * nivel 3.3 V -> 5 V */
#define PIN_MAX_DIN           GPIO_NUM_23
#define PIN_MAX_CLK           GPIO_NUM_18
#define PIN_MAX_CS            GPIO_NUM_5

/* 2 x 74HCT595 en cadena, alimentados a 5 V (aceptan los 3.3 V del ESP32 en
 * sus entradas). LEDs con resistencias de 470 Ω.
 * Primer chip: Q0..Q3 gasolina, Q4..Q6 RGB, Q7 direccional izquierda.
 * Segundo chip: Q0 direccional derecha. */
#define PIN_595_DATA          GPIO_NUM_13
#define PIN_595_CLK           GPIO_NUM_14
#define PIN_595_LATCH         GPIO_NUM_27

/* PCF8574 del teclado (I2C) */
#define PIN_I2C_SDA           GPIO_NUM_21
#define PIN_I2C_SCL           GPIO_NUM_22

/* Potenciómetro de gasolina (ADC1 canal 4) */
#define PIN_POT_GASOLINA      GPIO_NUM_32

/* Solo entrada, pull-up externa de 10 kΩ */
#define PIN_SW_IGNICION       GPIO_NUM_34
#define PIN_SW_SENTIDO        GPIO_NUM_35
#define PIN_SW_DIR_IZQ        GPIO_NUM_36
#define PIN_SW_DIR_DER        GPIO_NUM_39

/* Pull-up interna */
#define PIN_SW_INTERMITENTES  GPIO_NUM_33
#define PIN_BTN_ARRANQUE      GPIO_NUM_16
#define PIN_BTN_FRENO_DEL     GPIO_NUM_17
#define PIN_BTN_FRENO_TRA     GPIO_NUM_19
#define PIN_BTN_CAMBIO        GPIO_NUM_4
#define PIN_BTN_RESET         GPIO_NUM_15

/* LED integrado de la placa */
#define PIN_LED_VIDA          GPIO_NUM_2

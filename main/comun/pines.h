/*
 * pines.h — Asignación de GPIO (equivale al archivo de restricciones XDC).
 *
 * VERSIÓN DEL AVANCE CON HARDWARE DISPONIBLE (rama avance-hardware).
 * El mapa original del Segundo Avance (servos, MAX7219, 74HCT595, teclado)
 * está en la rama main. Aquí se cambian las salidas por lo que tenemos:
 *   - 2 servos      -> 1 motor DC con reductor, encendido por un relé
 *   - 74HCT595      -> 7 LEDs conectados directo a GPIO
 *   - MAX7219       -> LCD 16x2 (con módulo I2C o en paralelo de 4 bits)
 *   - botón de cambio + switch de sentido -> eje Y del joystick
 *   - teclado 4x4   -> no se conecta en este avance
 *
 * No se usan GPIO 0, 1, 3, 6 a 11 ni 12 (arranque, flash y USB).
 * GPIO 34, 35, 36 y 39 son solo de entrada y NO tienen pull-up interna.
 */
#pragma once

#include "driver/gpio.h"

/* ===================== Tipo de LCD =====================
 * 1 = la LCD trae atrás un módulo I2C de 4 pines (GND, VCC, SDA, SCL)
 * 0 = la LCD se conecta con sus 16 pines (modo de 4 bits: RS, E, D4..D7)
 * Es lo ÚNICO que hay que cambiar según la LCD que tengan. */
#define LCD_I2C               0

/* ===================== Entradas ===================== */
/* Solo entrada, pull-up EXTERNA de 10 kΩ a 3.3 V. Botón de ignición a GND
 * (o cable/jumper si IGNICION_CON_BOTON = 0 en config.h) */
#define PIN_SW_IGNICION       GPIO_NUM_34

/* Eje Y del joystick (ADC1 canal 7). Joystick alimentado a 3.3 V */
#define PIN_JOY_Y             GPIO_NUM_35

/* Potenciómetro de gasolina de 10 kΩ (ADC1 canal 4), extremos a 3.3 V y GND */
#define PIN_POT_GASOLINA      GPIO_NUM_32

/* Pull-up interna: el componente cierra a GND */
#define PIN_BTN_ARRANQUE      GPIO_NUM_16   /* botón del joystick (SW) */
#define PIN_BTN_FRENO_DEL     GPIO_NUM_17   /* microinterruptor: COM a GND, NO al pin */

/* Freno trasero: módulo TCRT5000 (su salida DO baja a 0 V al detectar la mano).
 * El módulo trae su propia resistencia de pull-up. */
#if LCD_I2C
#define PIN_BTN_FRENO_TRA     GPIO_NUM_19
#else
#define PIN_BTN_FRENO_TRA     GPIO_NUM_39   /* se mueve para liberar el 19 para la LCD */
#endif

/* ===================== Salidas ===================== */
/* Entrada IN del módulo de relé que enciende el motor */
#define PIN_RELE_MOTOR        GPIO_NUM_25

/* LEDs directos, cada uno con resistencia de 220 a 470 Ω a GND */
#define PIN_LED_GAS_1         GPIO_NUM_13   /* rojo */
#define PIN_LED_GAS_2         GPIO_NUM_14   /* amarillo */
#define PIN_LED_GAS_3         GPIO_NUM_27   /* verde */
#define PIN_LED_GAS_4         GPIO_NUM_26   /* verde */
#define PIN_LED_ESTADO_R      GPIO_NUM_18   /* "RGB" con 3 LEDs separados */
#define PIN_LED_ESTADO_G      GPIO_NUM_23
#define PIN_LED_ESTADO_B      GPIO_NUM_5

/* LED integrado de la placa */
#define PIN_LED_VIDA          GPIO_NUM_2

/* ===================== LCD 16x2 ===================== */
#if LCD_I2C
/* Bus I2C compartido por la LCD y (en la versión final) el teclado */
#define PIN_I2C_SDA           GPIO_NUM_21
#define PIN_I2C_SCL           GPIO_NUM_22
#else
/* LCD en paralelo, 4 bits. RW (pin 5) va a GND: solo se escribe.
 * V0 (pin 3) al cursor de un potenciómetro de 5 kΩ para el contraste. */
#define PIN_LCD_RS            GPIO_NUM_19
#define PIN_LCD_E             GPIO_NUM_4
#define PIN_LCD_D4            GPIO_NUM_15
#define PIN_LCD_D5            GPIO_NUM_33
#define PIN_LCD_D6            GPIO_NUM_21
#define PIN_LCD_D7            GPIO_NUM_22
#endif

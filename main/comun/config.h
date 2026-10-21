/*
 * config.h — Valores numéricos del diseño.
 *
 * Corresponden al Segundo Avance del proyecto (18 oct 2026), Tablas 1 y C1.
 * Los marcados con [NUEVO] no aparecen en ese documento y están explicados
 * en docs/04_cambios_al_documento.md.
 */
#pragma once

#include "sdkconfig.h"

#if CONFIG_FREERTOS_HZ != 1000
#error "El ciclo de 10 ms necesita CONFIG_FREERTOS_HZ=1000 (ver sdkconfig.defaults)"
#endif

/* ===================== Ciclo principal ===================== */
#define CICLO_MS                10
#define CICLO_PRIORIDAD         5
#define CICLO_PILA              6144
#define CICLO_NUCLEO            1       /* APP_CPU; el núcleo 0 queda para el sistema */
#define REPORTE_CICLOS          500     /* reporte de estado y tiempos cada 5 s */

/* Consola: buffers del driver de UART para que ESP_LOG no bloquee el ciclo */
#define CONSOLA_RX_BUFFER       256
#define CONSOLA_TX_BUFFER       4096

/* ===================== Entradas ===================== */
#define ANTIRREBOTE_LECTURAS    2       /* 2 lecturas iguales x 10 ms = 20 ms */

/* Potenciómetro: lectura cruda del ADC de 12 bits (0..4095).
 * ADC_MIN/ADC_MAX se calibran con el potenciómetro real. */
#define ADC_MIN                 0
#define ADC_MAX                 4095
#define ADC_PROMEDIO            8       /* promedio móvil de 8 lecturas */

/* Gasolina: 5 zonas, porcentaje mínimo de cada una. Zona 0 = vacío,
 * 1 = reserva, 2..4 = barra normal. Umbrales por calibrar con la pieza real. */
#define GAS_UMBRAL_PCT          { 0, 5, 25, 50, 75 }
#define GAS_HISTERESIS_PCT      3
#define GAS_NIVEL_VACIO         0
#define GAS_NIVEL_RESERVA       1
#define GAS_NIVEL_MAX           4

/* ===================== Teclado ===================== */
#define PCF8574_DIRECCION       0x20
#define PCF8574A_DIRECCION      0x38    /* [NUEVO] respaldo si el módulo es la versión "A" */
#define I2C_HZ                  100000  /* el PCF8574 solo admite 100 kHz */
#define TECLADO_ESPERA_MS       2       /* tiempo máximo de cada transferencia I2C (una dura ~0.2 ms) */
#define TECLADO_PAUSA_CICLOS    100     /* tras un error de I2C se reintenta en 1 s, no en cada ciclo */

/* ===================== Clave del modo de pruebas ===================== */
#define CLAVE_PRUEBAS           "2026"  /* se confirma con '#', '*' borra */
#define CLAVE_LONGITUD          4

/* ===================== Máquina de estados ===================== */
#define MARCHA_MIN              1
#define MARCHA_MAX              4
#define ARRANQUE_CICLOS         100     /* el arranque dura 1 s */

/* ===================== RPM ===================== */
#define RPM_RALENTI             1000
#define RPM_MARCHA              { 0, 3000, 5000, 7000, 9000 }   /* índice = marcha */
#define RPM_RAMPA_POR_CICLO     20      /* 2000 RPM/s x 10 ms */

/* ===================== Velocidad de las ruedas ===================== */
/* La velocidad va de 0 a 10000 (centésimas de %; 10000 = 100.00 % = marcha 4
 * = ±400 µs). Con esta escala 75 %/s, 25 %/s, 50 %/s y 100 %/s dan pasos
 * enteros por ciclo de 10 ms (75, 25, 50 y 100). Los frenos restan un
 * porcentaje de la velocidad MÁXIMA por segundo. */
#define VEL_MAX                 10000
#define VEL_POR_MARCHA          2500    /* marcha 1..4 -> 2500, 5000, 7500, 10000 */
#define ACEL_POR_CICLO          50      /* [NUEVO] subida 50 %/s */
#define DESACEL_MOTOR_POR_CICLO 30      /* [NUEVO] sin freno (bajar marcha, sin gasolina): 30 %/s */
#define FRENO_DEL_POR_CICLO     75      /* delantero: -75 %/s */
#define FRENO_TRA_POR_CICLO     25      /* trasero: -25 %/s */
#define FRENO_ABS_POR_CICLO     100     /* ambos (ABS): -100 %/s -> alto en 1 s o menos */

/* ===================== Servos ===================== */
#define SERVO_FREQ_HZ           50
#define SERVO_BITS              14      /* 16384 cuentas en 20 ms */
#define SERVO_PERIODO_US        20000
#define SERVO_NEUTRO_IZQ_US     1500    /* calibrar con cada servo real */
#define SERVO_NEUTRO_DER_US     1500
#define SERVO_RANGO_US          400     /* ±400 µs en marcha 4 */
#define PRUEBA_SERVOS_CICLOS    200     /* rutina 3: 2 s en cada marcha */
/* Mínimo de ciclos entre escrituras al LEDC (30 ms > periodo de 20 ms): en el
 * ESP32 ledc_update_duty() espera a que se aplique el duty anterior. */
#define SERVO_CICLOS_ENTRE_ESCRITURAS 3

/* ===================== Kilometraje ===================== */
#define METROS_POR_VUELTA       125     /* 8 vueltas simuladas = 1 km */
/* [NUEVO] vueltas por segundo a velocidad máxima (FS90R ~120 rpm = 2 rps).
 * Los 16 km (128 vueltas) llegan en ~65 s en marcha 4 y en ~4.3 min en marcha 1. */
#define RUEDA_RPS_MAX_X100      200
#define SERVICIO_CADA_METROS    16000   /* aviso de servicio cada 16 km: 16, 32, 48... */

/* ===================== Display ===================== */
#define MAX7219_SPI_HZ          1000000 /* [NUEVO] limitado por el convertidor de nivel */
#define MAX7219_BRILLO          4       /* 0..15 */
#define DISPLAY_REINICIO_CICLOS 200     /* [NUEVO] reconfigura el MAX7219 cada 2 s */

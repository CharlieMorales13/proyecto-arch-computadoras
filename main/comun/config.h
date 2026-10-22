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

/* ===================== Motor (relé) ===================== */
/* [AVANCE] El relé solo enciende o apaga: el motor gira mientras velocidad > 0.
 * La velocidad se sigue calculando igual que con los servos (rampa y frenos). */
#define RELE_ACTIVO_EN_BAJO     1       /* 1: módulo de relé que se activa con 0 V (el más común).
                                         * 0: se activa con 3.3 V (o relé con transistor NPN). */
#define RELE_CICLOS_MIN         20      /* mínimo 200 ms encendido antes de volver a conmutar */
#define PRUEBA_MOTOR_CICLOS     200     /* rutina 3: 2 s en cada marcha */

/* ===================== Kilometraje ===================== */
#define METROS_POR_VUELTA       125     /* 8 vueltas simuladas = 1 km */
/* [NUEVO] vueltas por segundo a velocidad máxima (FS90R ~120 rpm = 2 rps).
 * Los 16 km (128 vueltas) llegan en ~65 s en marcha 4 y en ~4.3 min en marcha 1. */
#define RUEDA_RPS_MAX_X100      200
#define SERVICIO_CADA_METROS    16000   /* aviso de servicio cada 16 km: 16, 32, 48... */

/* ===================== Ignición ===================== */
/* [AVANCE] 1: botón de presionar (push) en GPIO 34; cada pulsación enciende o
 *             apaga la ignición, como un botón de encendido.
 *          0: interruptor o cable que se queda puesto (encendido mientras esté a GND). */
#define IGNICION_CON_BOTON      1
/* Tiempo mínimo entre dos cambios de la ignición. Evita que un botón con
 * rebote largo o un sensor infrarrojo que "titila" en el borde de su alcance
 * la enciendan y apaguen varias veces con una sola pulsación. */
#define IGNICION_BLOQUEO_CICLOS 50      /* 500 ms */

/* ===================== Joystick (cambio de marcha) ===================== */
/* [AVANCE] Eje Y en el ADC (0..4095; en reposo queda cerca de 1900).
 * Llevarlo a un extremo pide UN cambio de marcha; hay que regresarlo al
 * centro para pedir el siguiente. Calibrar con el valor que imprime el monitor. */
#define JOY_EXTREMO_ALTO        3300    /* por encima: extremo "alto" */
#define JOY_EXTREMO_BAJO        800     /* por debajo: extremo "bajo" */
#define JOY_CENTRO_MIN          1300    /* entre estos dos: centro (rearma el cambio) */
#define JOY_CENTRO_MAX          2700
#define JOY_ALTO_SUBE           1       /* 1: el extremo alto sube la marcha; 0 si en su joystick es al revés */

/* ===================== LCD 16x2 ===================== */
/* [AVANCE] Direcciones típicas del módulo I2C (PCF8574 = 0x27, PCF8574A = 0x3F).
 * Si no responde en ninguna, se buscan las demás 0x20..0x27 y 0x38..0x3F. */
#define LCD_DIRECCION           0x27
#define LCD_DIRECCION_A         0x3F
#define LCD_I2C_HZ              100000
#define LCD_ESPERA_MS           10      /* tiempo máximo de cada transferencia I2C */
#define LCD_CARACTERES_POR_CICLO 6      /* ~2.6 ms por ciclo a 100 kHz; la pantalla completa en ~60 ms */
#define LCD_PAUSA_CICLOS        100     /* tras un error de I2C se reintenta en 1 s */
#define LCD_REINICIO_CICLOS     200     /* re-sincroniza la LCD cada 2 s (por ruido del motor y el relé) */

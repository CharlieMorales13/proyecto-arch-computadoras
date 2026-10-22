/*
 * estado.h — Estructura EstadoECU: el "banco de registros" de la ECU.
 *
 * Una sola tarea de FreeRTOS llama a todos los módulos en orden cada 10 ms
 * y les pasa un puntero a esta estructura. Como solo hay una tarea, no hay
 * condiciones de carrera ni hacen falta mutex.
 *
 * REGLA: cada campo tiene un único módulo que lo escribe (anotado en cada
 * bloque); los demás solo lo leen. La única excepción es el arranque:
 * persistencia_iniciar() carga marcha, km_m y flag_servicio desde la NVS
 * antes del primer ciclo, como el valor de reset de un registro.
 *
 * Orden de ejecución dentro de cada ciclo (main.c):
 *   tiempos -> entradas -> teclado -> fsm_diag -> fsm_ecu -> km_rpm
 *   -> persistencia -> servos -> salidas_leds -> display
 * Un módulo que lee un campo escrito más abajo en la lista ve el valor del
 * ciclo anterior (igual que leer un registro en un diseño síncrono).
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

/* Estados de la máquina principal */
typedef enum {
    ESTADO_APAGADO = 0,
    ESTADO_BLOQUEADO,
    ESTADO_NEUTRAL,
    ESTADO_ARRANQUE,
    ESTADO_EN_MARCHA,
    ESTADO_FRENADO,
    ESTADO_MODO_PRUEBAS,
    ESTADO_SIN_COMBUSTIBLE,
    ESTADO_TOTAL
} estado_ecu_t;

/* Rutinas del modo de pruebas (tecla que las elige) */
typedef enum {
    RUTINA_MENU = 0,        /* recién desbloqueado, esperando tecla */
    RUTINA_LEDS = 1,        /* '1': LEDs y RGB */
    RUTINA_DISPLAY = 2,     /* '2': display 88888888 */
    RUTINA_SERVOS = 3,      /* '3': servos en cada marcha */
    RUTINA_SERVICIO = 4,    /* '4': desactiva el aviso de servicio sin borrar km */
} rutina_t;

typedef struct {
    /* ---------- tiempos.c ---------- */
    uint32_t ciclos;            /* ciclos de 10 ms desde el arranque */
    bool tick_100ms;            /* marcas: verdaderas durante UN ciclo */
    bool tick_250ms;            /* para el parpadeo de 2 Hz */
    bool tick_500ms;
    bool tick_1s;
    bool fase_1hz;              /* ondas cuadradas para parpadeos */
    bool fase_2hz;
    bool fase_5hz;

    /* ---------- entradas.c ---------- */
    bool ign_on;                /* condiciones (nivel) */
    bool sentido_cambio;        /* true = subir marcha, false = bajar */
    bool dir_izq;
    bool dir_der;
    bool intermitentes;
    bool freno_del;
    bool freno_tra;
    bool btn_arranque;          /* eventos (flanco de un ciclo) */
    bool btn_cambio;
    bool btn_reset;
    uint16_t adc_gasolina;      /* lectura cruda promediada, 0..4095 */
    uint8_t nivel_gasolina;     /* 0 = vacío, 1 = reserva, 2..4 */

    /* ---------- teclado.c ---------- */
    char tecla;
    bool tecla_valida;          /* verdadera durante UN ciclo por pulsación */

    /* ---------- fsm_diag.c ---------- */
    bool sys_unlocked;
    uint8_t clave_digitos;      /* dígitos tecleados, para mostrarlos en el display */
    uint8_t rutina;             /* rutina_t, válido en ESTADO_MODO_PRUEBAS */
    bool quitar_servicio;       /* petición (UN ciclo) de la rutina 4; la atiende km_rpm */

    /* ---------- fsm_ecu.c ---------- */
    estado_ecu_t estado;
    uint8_t marcha;             /* 1..4 */
    uint32_t ciclos_en_estado;

    /* ---------- km_rpm.c ---------- */
    uint16_t rpm;
    uint32_t km_m;              /* kilometraje en metros */
    bool flag_servicio;

    /* ---------- servos.c (en el avance: motor DC por relé) ---------- */
    int16_t velocidad;          /* 0..10000 = 0..100.00 % de la velocidad máxima */

    /* ---------- salidas_leds.c ---------- */
    uint16_t bits_leds;         /* imagen de las salidas de LEDs (bit i = LED i) */

    /* ---------- display.c ---------- */
    char lcd[2][16];            /* texto que debe verse en la LCD 16x2 (renglón, columna) */
} EstadoECU;

const char *estado_nombre(estado_ecu_t e);

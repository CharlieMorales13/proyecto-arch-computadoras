/*
 * main.c — Módulo estructural (equivale a top_module en VHDL).
 *
 * Inicializa los periféricos, carga el estado guardado y corre el ciclo de
 * 10 ms con vTaskDelayUntil(). En cada ciclo llama a los módulos en orden;
 * no toma decisiones propias.
 *
 * Además mide con esp_timer_get_time() cuánto tarda cada módulo y cada 5 s
 * imprime el peor tiempo de cada uno, el peor ciclo completo y cuántos
 * ciclos se pasaron de 10 ms.
 */
#include <stdio.h>

#include "config.h"
#include "display.h"
#include "driver/uart.h"
#include "driver/uart_vfs.h"
#include "entradas.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "estado.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "fsm_diag.h"
#include "fsm_ecu.h"
#include "km_rpm.h"
#include "persistencia.h"
#include "salidas_leds.h"
#include "servos.h"
#include "teclado.h"
#include "tiempos.h"

static const char *TAG = "main";

/* Banco de registros de la ECU. Solo lo usa la tarea del ciclo. */
static EstadoECU s_ecu;

/* Módulos en el orden en que se ejecutan (entradas -> control -> memoria -> salidas) */
typedef void (*paso_t)(EstadoECU *e);

typedef struct {
    const char *nombre;
    paso_t paso;
} modulo_t;

static const modulo_t MODULOS[] = {
    { "tiempos",      tiempos_actualizar },
    { "entradas",     entradas_actualizar },
    { "teclado",      teclado_actualizar },
    { "fsm_diag",     fsm_diag_actualizar },
    { "fsm_ecu",      fsm_ecu_actualizar },
    { "km_rpm",       km_rpm_actualizar },
    { "persistencia", persistencia_actualizar },
    { "servos",       servos_actualizar },
    { "salidas_leds", salidas_leds_actualizar },
    { "display",      display_actualizar },
};
#define NUM_MODULOS  (sizeof(MODULOS) / sizeof(MODULOS[0]))

static void reportar(const EstadoECU *e, const int64_t peor_modulo[], int64_t peor_ciclo,
                     uint32_t sobrepasos)
{
    ESP_LOGI(TAG, "estado=%s marcha=%u rpm=%u km=%lu m gas=%u (adc %u)",
             estado_nombre(e->estado), e->marcha, e->rpm, (unsigned long)e->km_m,
             e->nivel_gasolina, e->adc_gasolina);

    char linea[256];
    int n = snprintf(linea, sizeof(linea), "peor us:");
    for (size_t i = 0; i < NUM_MODULOS && n < (int)sizeof(linea); i++) {
        n += snprintf(linea + n, sizeof(linea) - n, " %s=%lld", MODULOS[i].nombre, peor_modulo[i]);
    }
    ESP_LOGI(TAG, "%s", linea);
    ESP_LOGI(TAG, "ciclo peor=%lld us, ciclos de mas de %d ms=%lu",
             peor_ciclo, CICLO_MS, (unsigned long)sobrepasos);
}

static void ciclo_ecu(void *arg)
{
    EstadoECU *e = (EstadoECU *)arg;
    int64_t peor_modulo[NUM_MODULOS] = { 0 };
    int64_t peor_ciclo = 0;
    uint32_t sobrepasos = 0;
    TickType_t ultimo = xTaskGetTickCount();

    for (;;) {
        const int64_t inicio = esp_timer_get_time();

        for (size_t i = 0; i < NUM_MODULOS; i++) {
            const int64_t t0 = esp_timer_get_time();
            MODULOS[i].paso(e);
            const int64_t dt = esp_timer_get_time() - t0;
            if (dt > peor_modulo[i]) {
                peor_modulo[i] = dt;
            }
        }

        const int64_t duracion = esp_timer_get_time() - inicio;
        if (duracion > peor_ciclo) {
            peor_ciclo = duracion;
        }
        if (e->ciclos % REPORTE_CICLOS == 0) {
            reportar(e, peor_modulo, peor_ciclo, sobrepasos);
            for (size_t i = 0; i < NUM_MODULOS; i++) {
                peor_modulo[i] = 0;
            }
            peor_ciclo = 0;
            sobrepasos = 0;
        }

        /* vTaskDelayUntil() solo bloquea si la hora de despertar aún no llega.
         * Si el ciclo se pasó de 10 ms regresa sin bloquear; si eso se repite,
         * esta tarea nunca suelta la CPU 1, IDLE1 no corre y el task watchdog
         * se dispara. En ese caso se cede un tick y se toma la hora actual
         * como nueva referencia, así la tarea bloquea en TODOS los ciclos. */
        if (xTaskDelayUntil(&ultimo, pdMS_TO_TICKS(CICLO_MS)) == pdFALSE) {
            sobrepasos++;
            vTaskDelay(1);
            ultimo = xTaskGetTickCount();
        }
    }
}

/* Con el driver de UART instalado, ESP_LOG copia el texto a un buffer y
 * regresa; sin él, cada carácter espera su turno en la FIFO de 128 bytes a
 * 115200 baudios (~87 us por carácter) dentro del ciclo de 10 ms. */
static void consola_con_buffer(void)
{
    const uart_port_t uart = (uart_port_t)CONFIG_ESP_CONSOLE_UART_NUM;
    if (uart_driver_install(uart, CONSOLA_RX_BUFFER, CONSOLA_TX_BUFFER, 0, NULL, 0) == ESP_OK) {
        uart_vfs_dev_use_driver(uart);
    } else {
        ESP_LOGW(TAG, "no se pudo instalar el driver de UART; los logs bloquearán el ciclo");
    }
}

void app_main(void)
{
    consola_con_buffer();
    ESP_LOGI(TAG, "ECU digital para motocicleta - ESP32");

    /* El estado inicial: todo en cero salvo lo que se recupera de la NVS */
    s_ecu.estado = ESTADO_APAGADO;
    ESP_ERROR_CHECK(persistencia_iniciar(&s_ecu));

    ESP_ERROR_CHECK(entradas_iniciar());
    ESP_ERROR_CHECK(salidas_leds_iniciar());
    ESP_ERROR_CHECK(servos_iniciar());

    /* Teclado y display no detienen a la ECU si faltan: se avisa y se sigue */
    if (teclado_iniciar() != ESP_OK) {
        ESP_LOGW(TAG, "sin teclado: el modo de pruebas no estará disponible");
    }
    if (display_iniciar() != ESP_OK) {
        ESP_LOGW(TAG, "sin display");
    }

    xTaskCreatePinnedToCore(ciclo_ecu, "ciclo_ecu", CICLO_PILA, &s_ecu,
                            CICLO_PRIORIDAD, NULL, CICLO_NUCLEO);
}

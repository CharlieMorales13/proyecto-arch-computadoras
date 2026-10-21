#include "display.h"

#include <stdio.h>
#include <string.h>

#include "config.h"
#include "driver/spi_master.h"
#include "esp_check.h"
#include "esp_log.h"
#include "pines.h"

static const char *TAG = "display";

/* Registros del MAX7219 */
#define REG_DIGITO_0    0x01    /* 0x01..0x08 */
#define REG_DECODE      0x09
#define REG_INTENSIDAD  0x0A
#define REG_SCAN_LIMIT  0x0B
#define REG_SHUTDOWN    0x0C
#define REG_TEST        0x0F

#define SEG_PUNTO       0x80

static spi_device_handle_t s_spi;
static bool s_habilitado;

/* Una palabra de 16 bits: dirección del registro y dato */
static esp_err_t max_escribir(uint8_t reg, uint8_t dato)
{
    spi_transaction_t t = {
        .length = 16,
        .flags = SPI_TRANS_USE_TXDATA,
        .tx_data = { reg, dato },
    };
    return spi_device_polling_transmit(s_spi, &t);
}

static void configurar_max(void)
{
    max_escribir(REG_TEST, 0);
    max_escribir(REG_DECODE, 0);            /* segmentos directos, sin BCD */
    max_escribir(REG_SCAN_LIMIT, 7);        /* 8 dígitos */
    max_escribir(REG_INTENSIDAD, MAX7219_BRILLO);
    max_escribir(REG_SHUTDOWN, 1);          /* operación normal */
}

esp_err_t display_iniciar(void)
{
    const spi_bus_config_t bus = {
        .mosi_io_num = PIN_MAX_DIN,
        .miso_io_num = -1,
        .sclk_io_num = PIN_MAX_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 2,
    };
    ESP_RETURN_ON_ERROR(spi_bus_initialize(SPI3_HOST, &bus, SPI_DMA_DISABLED), TAG, "bus SPI3");

    const spi_device_interface_config_t dev = {
        .clock_speed_hz = MAX7219_SPI_HZ,
        .mode = 0,                  /* el MAX7219 captura el dato en el flanco de subida */
        .spics_io_num = PIN_MAX_CS, /* al subir CS el dato se carga al registro */
        .queue_size = 1,
    };
    ESP_RETURN_ON_ERROR(spi_bus_add_device(SPI3_HOST, &dev, &s_spi), TAG, "MAX7219");

    s_habilitado = true;
    configurar_max();
    for (int i = 0; i < 8; i++) {
        max_escribir(REG_DIGITO_0 + i, 0);
    }
    return ESP_OK;
}

/* Bits del MAX7219 sin decodificar: DP A B C D E F G (bit 7..0) */
static uint8_t segmentos(char c)
{
    switch (c) {
    case '0': return 0x7E;
    case '1': return 0x30;
    case '2': return 0x6D;
    case '3': return 0x79;
    case '4': return 0x33;
    case '5': case 'S': return 0x5B;
    case '6': return 0x5F;
    case '7': return 0x70;
    case '8': return 0x7F;
    case '9': return 0x7B;
    case 'A': return 0x77;
    case 'b': return 0x1F;
    case 'E': return 0x4F;
    case 'I': return 0x30;
    case 'n': return 0x15;
    case 'P': return 0x67;
    case 'r': return 0x05;
    case 'u': return 0x1C;
    case '-': return 0x01;
    default:  return 0x00;  /* espacio */
    }
}

/* Convierte texto a 8 dígitos; un '.' enciende el punto del dígito anterior. */
static void texto_a_digitos(const char *txt, uint8_t dig[8])
{
    memset(dig, 0, 8);
    int pos = 0;
    for (const char *p = txt; *p != '\0'; p++) {
        if (*p == '.') {
            if (pos > 0) {
                dig[pos - 1] |= SEG_PUNTO;
            }
        } else if (pos < 8) {
            dig[pos++] = segmentos(*p);
        }
    }
}

static void componer_texto(const EstadoECU *e, char *txt, size_t n)
{
    switch (e->estado) {
    case ESTADO_APAGADO:
        txt[0] = '\0';
        break;

    case ESTADO_ARRANQUE:
    case ESTADO_EN_MARCHA:
    case ESTADO_FRENADO:
        snprintf(txt, n, "%4u   %u", e->rpm, e->marcha);
        break;

    case ESTADO_BLOQUEADO:
        snprintf(txt, n, "PIn %.*s", e->clave_digitos, "----");
        break;

    case ESTADO_MODO_PRUEBAS:
        if (e->rutina == RUTINA_DISPLAY) {
            snprintf(txt, n, "8.8.8.8.8.8.8.8.");
        } else if (e->rutina == RUTINA_MENU) {
            snprintf(txt, n, "PruEbA -");
        } else {
            snprintf(txt, n, "PruEbA %u", e->rutina);
        }
        break;

    default: {  /* NEUTRAL y SIN_COMBUSTIBLE: kilómetros con una décima */
        const unsigned long decimas = e->km_m / 100u;
        snprintf(txt, n, "%c%6lu.%lu", e->flag_servicio ? 'S' : ' ',
                 decimas / 10u, decimas % 10u);
        break;
    }
    }
}

void display_actualizar(EstadoECU *e)
{
    if (!s_habilitado || !e->tick_100ms) {
        return;
    }

    /* Cada 2 s se reconfigura el chip: si el ruido de los servos alteró un
     * registro de control, el display se recupera solo. */
    const bool reconfigurar = (e->ciclos % DISPLAY_REINICIO_CICLOS) == 0;
    if (reconfigurar) {
        configurar_max();
    }

    char txt[32];
    uint8_t nuevos[8];
    componer_texto(e, txt, sizeof(txt));
    texto_a_digitos(txt, nuevos);

    if (reconfigurar || memcmp(nuevos, e->digitos, sizeof(nuevos)) != 0) {
        /* DIGIT0 es el dígito de la derecha en los módulos comunes */
        for (int pos = 0; pos < 8; pos++) {
            max_escribir(REG_DIGITO_0 + (7 - pos), nuevos[pos]);
        }
        memcpy(e->digitos, nuevos, sizeof(nuevos));
    }
}

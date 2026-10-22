#include "teclado.h"

#include "bus_i2c.h"
#include "config.h"
#include "driver/i2c_master.h"
#include "esp_check.h"
#include "esp_log.h"

static const char *TAG = "teclado";

static const char MAPA[4][4] = {
    { '1', '2', '3', 'A' },
    { '4', '5', '6', 'B' },
    { '7', '8', '9', 'C' },
    { '*', '0', '#', 'D' },
};

static i2c_master_bus_handle_t s_bus;
static i2c_master_dev_handle_t s_pcf;
static bool s_habilitado;
static uint16_t s_pausa;            /* ciclos que faltan para reintentar tras un error */
static bool s_en_error;             /* para avisar una sola vez por falla */

static uint8_t s_fila;              /* estado de la FSM: fila que se barre */
static char s_tecla_barrido;        /* tecla encontrada en el barrido en curso */
static char s_tecla_anterior;       /* resultado del barrido completo anterior */
static char s_tecla_entregada;      /* última tecla ya entregada */

static esp_err_t pcf_escribir(uint8_t v)
{
    return i2c_master_transmit(s_pcf, &v, 1, TECLADO_ESPERA_MS);
}

static esp_err_t pcf_leer(uint8_t *v)
{
    return i2c_master_receive(s_pcf, v, 1, TECLADO_ESPERA_MS);
}

esp_err_t teclado_iniciar(void)
{
    /* [AVANCE] El bus se comparte con la LCD I2C (ver bus_i2c.h) */
    ESP_RETURN_ON_ERROR(bus_i2c_obtener(&s_bus), TAG, "bus I2C");

    uint16_t dir = 0;
    if (i2c_master_probe(s_bus, PCF8574_DIRECCION, 20) == ESP_OK) {
        dir = PCF8574_DIRECCION;
    } else if (i2c_master_probe(s_bus, PCF8574A_DIRECCION, 20) == ESP_OK) {
        dir = PCF8574A_DIRECCION;
    }
    if (dir == 0) {
        ESP_LOGE(TAG, "el PCF8574 no responde en 0x%02X ni 0x%02X; teclado deshabilitado",
                 PCF8574_DIRECCION, PCF8574A_DIRECCION);
        return ESP_ERR_NOT_FOUND;
    }

    const i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = dir,
        .scl_speed_hz = I2C_HZ,
    };
    ESP_RETURN_ON_ERROR(i2c_master_bus_add_device(s_bus, &dev_cfg, &s_pcf), TAG, "dispositivo");
    ESP_RETURN_ON_ERROR(pcf_escribir(0xFF), TAG, "escritura inicial");

    s_habilitado = true;
    ESP_LOGI(TAG, "PCF8574 en 0x%02X", dir);
    return ESP_OK;
}

void teclado_actualizar(EstadoECU *e)
{
    e->tecla_valida = false;
    if (!s_habilitado) {
        return;
    }
    if (s_pausa > 0) {
        s_pausa--;
        return;
    }

    /* 1. Barrer la fila actual: fila en 0, columnas en 1 (entradas con pull-up).
     *    Si el PCF8574 no contesta, el driver espera activamente hasta el
     *    timeout; para no repetir esa espera en cada ciclo de 10 ms se pausa
     *    el teclado y se reintenta en 1 s. */
    uint8_t v = 0xFF;
    if (pcf_escribir((uint8_t)~(1u << s_fila)) != ESP_OK || pcf_leer(&v) != ESP_OK) {
        if (!s_en_error) {
            ESP_LOGW(TAG, "el PCF8574 no responde; reintento cada %d ms",
                     TECLADO_PAUSA_CICLOS * CICLO_MS);
            s_en_error = true;
        }
        s_pausa = TECLADO_PAUSA_CICLOS;
        s_fila = 0;
        s_tecla_barrido = 0;
        return;
    }
    if (s_en_error) {
        ESP_LOGI(TAG, "el PCF8574 responde de nuevo");
        s_en_error = false;
    }
    const uint8_t columnas = (uint8_t)((~v & 0xF0) >> 4);
    if (columnas != 0 && s_tecla_barrido == 0) {
        s_tecla_barrido = MAPA[s_fila][__builtin_ctz(columnas)];
    }

    /* 2. Al terminar las 4 filas se evalúa el barrido completo */
    if (++s_fila < 4) {
        return;
    }
    s_fila = 0;
    const char tecla = s_tecla_barrido;
    s_tecla_barrido = 0;

    /* Antirrebote: igual en dos barridos seguidos y distinta de la entregada */
    if (tecla == s_tecla_anterior && tecla != s_tecla_entregada) {
        if (tecla != 0) {
            e->tecla = tecla;
            e->tecla_valida = true;
        }
        s_tecla_entregada = tecla;      /* 0 = se soltó; permite repetir la tecla */
    }
    s_tecla_anterior = tecla;
}

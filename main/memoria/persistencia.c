#include "persistencia.h"

#include "config.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs.h"
#include "nvs_flash.h"

static const char *TAG = "persistencia";

#define NVS_ESPACIO     "ecu"
#define CLAVE_MARCHA    "marcha"
#define CLAVE_KM        "km_m"
#define CLAVE_SERVICIO  "servicio"

/* Copia de lo que hay en flash, para escribir solo lo que cambió */
static uint8_t s_marcha;
static uint32_t s_km_m;
static bool s_servicio;
static bool s_ign_previa;

esp_err_t persistencia_iniciar(EstadoECU *e)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "partición NVS inválida, se formatea");
        ESP_RETURN_ON_ERROR(nvs_flash_erase(), TAG, "nvs_flash_erase");
        err = nvs_flash_init();
    }
    ESP_RETURN_ON_ERROR(err, TAG, "nvs_flash_init");

    /* Valores de fábrica */
    s_marcha = MARCHA_MIN;
    s_km_m = 0;
    s_servicio = false;

    nvs_handle_t h;
    if (nvs_open(NVS_ESPACIO, NVS_READONLY, &h) == ESP_OK) {
        uint8_t u8;
        if (nvs_get_u8(h, CLAVE_MARCHA, &u8) == ESP_OK) {
            s_marcha = u8;
        }
        nvs_get_u32(h, CLAVE_KM, &s_km_m);
        if (nvs_get_u8(h, CLAVE_SERVICIO, &u8) == ESP_OK) {
            s_servicio = u8 != 0;
        }
        nvs_close(h);
    } else {
        ESP_LOGI(TAG, "sin datos guardados, valores de fábrica");
    }

    /* Valores iniciales de los "registros" */
    e->marcha = s_marcha;
    e->km_m = s_km_m;
    e->flag_servicio = s_servicio;
    s_ign_previa = false;

    ESP_LOGI(TAG, "cargado: marcha=%u km=%lu m servicio=%d",
             s_marcha, (unsigned long)s_km_m, s_servicio);
    return ESP_OK;
}

static void guardar(const EstadoECU *e)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_ESPACIO, NVS_READWRITE, &h);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nvs_open: %s", esp_err_to_name(err));
        return;
    }
    if (e->marcha != s_marcha && nvs_set_u8(h, CLAVE_MARCHA, e->marcha) == ESP_OK) {
        s_marcha = e->marcha;
    }
    if (e->km_m != s_km_m && nvs_set_u32(h, CLAVE_KM, e->km_m) == ESP_OK) {
        s_km_m = e->km_m;
    }
    if (e->flag_servicio != s_servicio && nvs_set_u8(h, CLAVE_SERVICIO, e->flag_servicio) == ESP_OK) {
        s_servicio = e->flag_servicio;
    }
    err = nvs_commit(h);
    nvs_close(h);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "guardado: marcha=%u km=%lu m servicio=%d",
                 s_marcha, (unsigned long)s_km_m, s_servicio);
    } else {
        ESP_LOGE(TAG, "nvs_commit: %s", esp_err_to_name(err));
    }
}

static void reset_maestro(void)
{
    ESP_LOGW(TAG, "RESET MAESTRO: se borra la NVS y se reinicia");
    nvs_handle_t h;
    if (nvs_open(NVS_ESPACIO, NVS_READWRITE, &h) == ESP_OK) {
        nvs_erase_all(h);
        nvs_commit(h);
        nvs_close(h);
    }
    esp_restart();      /* arranca de nuevo con valores de fábrica */
}

void persistencia_actualizar(EstadoECU *e)
{
    /* Flanco de apagado de la ignición */
    if (s_ign_previa && !e->ign_on) {
        guardar(e);
    }
    s_ign_previa = e->ign_on;

    if (e->btn_reset) {
        if (e->ign_on) {
            ESP_LOGW(TAG, "reset maestro ignorado: apaga la ignición primero");
        } else {
            reset_maestro();
        }
    }
}

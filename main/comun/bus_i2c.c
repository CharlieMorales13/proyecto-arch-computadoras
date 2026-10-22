#include "bus_i2c.h"

#include "pines.h"

esp_err_t bus_i2c_obtener(i2c_master_bus_handle_t *bus)
{
#if LCD_I2C
    static i2c_master_bus_handle_t s_bus;
    static esp_err_t s_resultado = ESP_FAIL;
    static bool s_creado;

    if (!s_creado) {
        s_creado = true;
        const i2c_master_bus_config_t cfg = {
            .i2c_port = -1,
            .sda_io_num = PIN_I2C_SDA,
            .scl_io_num = PIN_I2C_SCL,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .flags.enable_internal_pullup = true,
        };
        s_resultado = i2c_new_master_bus(&cfg, &s_bus);
    }
    *bus = s_bus;
    return s_resultado;
#else
    *bus = NULL;
    return ESP_ERR_NOT_SUPPORTED;
#endif
}

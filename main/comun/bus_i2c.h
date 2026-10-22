/*
 * bus_i2c.h — Bus I2C compartido (SDA 21, SCL 22).
 *
 * [AVANCE] Antes solo el teclado usaba el bus y lo creaba él mismo. Ahora la
 * LCD con módulo I2C cuelga del mismo par de cables, así que el bus se crea
 * una sola vez aquí y cada módulo agrega su dispositivo (como un bus
 * compartido con varios esclavos).
 *
 * Con la LCD en paralelo (LCD_I2C = 0) los pines 21 y 22 son de la LCD y el
 * bus no existe: devuelve ESP_ERR_NOT_SUPPORTED.
 */
#pragma once

#include "driver/i2c_master.h"
#include "esp_err.h"

/* Crea el bus la primera vez y devuelve el mismo en las siguientes. */
esp_err_t bus_i2c_obtener(i2c_master_bus_handle_t *bus);

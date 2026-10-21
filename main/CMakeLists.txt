idf_component_register(
    SRCS
        "main.c"
        "comun/estado.c"
        "entradas/entradas.c"
        "entradas/teclado.c"
        "control/tiempos.c"
        "control/fsm_diag.c"
        "control/fsm_ecu.c"
        "control/km_rpm.c"
        "memoria/persistencia.c"
        "salidas/servos.c"
        "salidas/salidas_leds.c"
        "salidas/display.c"
    INCLUDE_DIRS
        "comun" "entradas" "control" "memoria" "salidas"
    PRIV_REQUIRES
        esp_driver_gpio esp_driver_ledc esp_driver_spi esp_driver_i2c esp_driver_uart
        esp_adc esp_timer nvs_flash
)

idf_component_register(
    SRCS
        "app_main.c"
        "fluval_protocol.c"
        "fluval_ble.c"
        "uart_console.c"
    INCLUDE_DIRS "."
    REQUIRES bt nvs_flash esp_event esp_timer driver freertos
)

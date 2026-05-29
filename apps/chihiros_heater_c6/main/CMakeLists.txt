idf_component_register(
    SRCS
        "main.c"
        "chihiros_heater_client.c"
        "chihiros_heater_console.c"
    INCLUDE_DIRS "."
    REQUIRES chihiros_heater_protocol bt nvs_flash esp_event esp_timer console esp_driver_uart
)


#include <stdio.h>

#include "esp_event.h"
#include "esp_log.h"
#include "esp_vfs_dev.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"

#include "driver/uart.h"

#include "fluval_ble.h"
#include "fluval_protocol.h"
#include "uart_console.h"

static const char *TAG = "fluval_main";

static void console_uart_init(void)
{
    const uart_config_t uart_config = {
        .baud_rate = CONFIG_ESP_CONSOLE_UART_BAUDRATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    ESP_ERROR_CHECK(uart_driver_install(CONFIG_ESP_CONSOLE_UART_NUM, 512, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(CONFIG_ESP_CONSOLE_UART_NUM, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(CONFIG_ESP_CONSOLE_UART_NUM, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE,
                                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    esp_vfs_dev_uart_use_driver(CONFIG_ESP_CONSOLE_UART_NUM);
}

void app_main(void)
{
    ESP_LOGI(TAG, "Fluval BLE Helper starting");

    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    console_uart_init();

    printf("\nFluval BLE Helper\n");
    printf("Target: %s\n", FLUVAL_TARGET_NAME);
    printf("Commands: HELP, FLUVAL READ, FLUVAL STATUS, FLUVAL MODE AUTO, FLUVAL MODE MANUAL, "
           "FLUVAL SETALL, FLUVAL SET\n\n");

    if (!fluval_protocol_run_selftests()) {
        ESP_LOGW(TAG, "Protocol selftests failed");
    } else {
        ESP_LOGI(TAG, "Protocol selftests passed");
    }

    ESP_ERROR_CHECK(fluval_ble_init());
    ESP_ERROR_CHECK(fluval_ble_start());
    uart_console_start();

    ESP_LOGI(TAG, "Ready. Type HELP for commands.");
}

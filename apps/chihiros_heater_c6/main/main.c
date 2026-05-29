#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_console.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_vfs_dev.h"
#include "nvs_flash.h"

#include "driver/uart.h"
#include "linenoise/linenoise.h"

#include "chihiros_heater_client.h"
#include "chihiros_heater_console.h"

static const char *TAG = "chihiros_main";

static void state_cb(const chihiros_heater_client_state_t *st, void *ctx)
{
    (void)ctx;
    ESP_LOGI(TAG, "state: connected=%d subscribed=%d stale=%d", st->connected, st->subscribed, st->stale);
}

static void console_init(void)
{
    setvbuf(stdin, NULL, _IONBF, 0);
    setvbuf(stdout, NULL, _IONBF, 0);

    const uart_config_t uart_config = {
        .baud_rate = CONFIG_ESP_CONSOLE_UART_BAUDRATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    ESP_ERROR_CHECK(uart_driver_install(CONFIG_ESP_CONSOLE_UART_NUM, 256, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(CONFIG_ESP_CONSOLE_UART_NUM, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(CONFIG_ESP_CONSOLE_UART_NUM, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE,
                                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

    esp_vfs_dev_uart_use_driver(CONFIG_ESP_CONSOLE_UART_NUM);

    esp_console_config_t console_config = {
        .max_cmdline_args = 8,
        .max_cmdline_length = 256,
    };
    ESP_ERROR_CHECK(esp_console_init(&console_config));

    linenoiseSetMultiLine(1);
    linenoiseSetMaxLineLen(256);
    linenoiseHistorySetMaxLen(50);
}

static void repl_task(void *arg)
{
    (void)arg;
    while (true) {
        char *line = linenoise("> ");
        if (line == NULL) {
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }

        if (strlen(line) > 0) {
            linenoiseHistoryAdd(line);
            int ret = 0;
            esp_console_run(line, &ret);
        }
        linenoiseFree(line);
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "Chihiros heater C6 BLE client starting");

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(nvs_flash_init());

    console_init();

    chihiros_heater_client_t *client = NULL;
    chihiros_heater_client_config_t cfg = {
        .name_prefix = "DYH1",
        .stale_timeout_ms = 10000,
        .keepalive_enabled = false,
        .keepalive_period_ms = 0,
        .min_setpoint_f = 50.0f,
        .max_setpoint_f = 95.0f,
    };

    if (!chihiros_heater_client_init(&client, &cfg, state_cb, NULL)) {
        ESP_LOGE(TAG, "client init failed");
        return;
    }
    chihiros_heater_client_start_task(client);
    chihiros_heater_console_register(client);

    xTaskCreate(repl_task, "repl", 4096, NULL, 5, NULL);

    ESP_LOGI(TAG, "Ready. Type `connect` then `status`.");
}


#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_console.h"
#include "esp_log.h"
#include "esp_vfs_dev.h"
#include "nvs_flash.h"

#include "driver/uart.h"
#include "linenoise/linenoise.h"

#include "bsp/esp32_p4_platform.h"

#include "co2/co2_schedule.h"
#include "co2/co2_gpio.h"
#include "feeder/feeder_schedule.h"
#include "feeder/feeder_actuator.h"
#include "net/shelly_console.h"
#include "net/time_sync.h"
#include "net/wifi_manager.h"
#include "scheduler/scheduler.h"
#include "shelly/shelly_manager.h"
#include "storage/settings_nvs.h"
#include "ui/ui.h"

static const char *TAG = "fishduino";

typedef struct {
    fishduino_settings_t settings;
    fishduino_co2_t co2;
    fishduino_feeder_t feeder;
    fishduino_ui_t *ui;
} fishduino_app_t;

static fishduino_app_t s_app;

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

    esp_vfs_dev_uart_use_driver(CONFIG_ESP_CONSOLE_UART_NUM);

    esp_console_config_t console_config = {
        .max_cmdline_args = 8,
        .max_cmdline_length = 256,
    };
    ESP_ERROR_CHECK(esp_console_init(&console_config));

    linenoiseSetMultiLine(1);
    linenoiseSetMaxLineLen(256);
    linenoiseHistorySetMaxLen(32);
}

static void repl_task(void *arg)
{
    (void)arg;
    while (true) {
        char *line = linenoise("fishduino> ");
        if (line == NULL) {
            vTaskDelay(pdMS_TO_TICKS(100));
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

static void scheduler_tick(const fishduino_time_snapshot_t *now, void *ctx)
{
    fishduino_app_t *app = (fishduino_app_t *)ctx;

    fishduino_settings_t *live = fishduino_shelly_manager_get_settings_mutable();
    app->settings = *live;
    fishduino_co2_apply_settings(&app->co2, &app->settings);
    fishduino_co2_tick(&app->co2, now);
    fishduino_shelly_co2_tick(&app->co2, now);
    fishduino_shelly_filter_alarm_tick();
    fishduino_feeder_tick(&app->feeder, now);

    if (app->ui == NULL) {
        return;
    }

    if (!bsp_display_lock(50)) {
        return;
    }

    fishduino_ui_update(app->ui, &app->co2, &app->feeder, &app->settings);
    bsp_display_unlock();
}

void app_main(void)
{
    ESP_LOGI(TAG, "Fishduino starting");

    fishduino_settings_load(&s_app.settings);
    fishduino_time_sync_init();
    fishduino_time_sync_apply_timezone(&s_app.settings);

    fishduino_co2_init(&s_app.co2, &s_app.settings);
    fishduino_feeder_init(&s_app.feeder, &s_app.settings);
    fishduino_shelly_manager_init(&s_app.settings);

    console_init();
    fishduino_shelly_console_register();
    xTaskCreate(repl_task, "console", 4096, NULL, 3, NULL);

    ESP_LOGI(TAG, "Init display + touch + LVGL (BSP)");
    lv_display_t *disp = bsp_display_start();
    if (disp == NULL) {
        ESP_LOGE(TAG, "bsp_display_start() failed");
        return;
    }

    bsp_display_backlight_on();

    fishduino_ui_t ui = {0};
    fishduino_ui_init(&ui);
    s_app.ui = &ui;

    fishduino_scheduler_start(scheduler_tick, &s_app);

    fishduino_wifi_init();
    if (fishduino_wifi_start_sta()) {
        ESP_LOGI(TAG, "Wi-Fi STA started");
    } else {
        ESP_LOGW(TAG, "Wi-Fi not available (stub or init failed); Shelly disabled until connected");
    }
}

#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"

#include "chihiros_heater_protocol.h"

static const char *TAG = "chihiros_test";

void app_main(void)
{
    ESP_LOGI(TAG, "Running Chihiros protocol selftests");

    bool ok = chihiros_protocol_run_selftests();
    if (!ok) {
        ESP_LOGE(TAG, "SELFTEST FAIL");
    } else {
        ESP_LOGI(TAG, "SELFTEST PASS");
    }

    // Keep the task alive so monitor output is readable.
    while (true) {
        fflush(stdout);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}


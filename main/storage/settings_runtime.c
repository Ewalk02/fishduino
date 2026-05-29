#include "settings_runtime.h"

#include <string.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static const char *TAG = "settings_rt";

static fishduino_settings_t s_settings;
static SemaphoreHandle_t s_mutex;
static bool s_ready;

void fishduino_settings_runtime_init(void)
{
    if (s_mutex == NULL) {
        s_mutex = xSemaphoreCreateMutex();
    }

    fishduino_settings_load(&s_settings);
    s_ready = true;
    ESP_LOGI(TAG, "Settings runtime ready");
}

bool fishduino_settings_get_snapshot(fishduino_settings_t *out)
{
    if (out == NULL || !s_ready || s_mutex == NULL) {
        return false;
    }

    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(500)) != pdTRUE) {
        return false;
    }
    *out = s_settings;
    xSemaphoreGive(s_mutex);
    return true;
}

bool fishduino_settings_update(fishduino_settings_mutator_fn fn, void *ctx, bool persist)
{
    if (fn == NULL || !s_ready || s_mutex == NULL) {
        return false;
    }

    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(2000)) != pdTRUE) {
        return false;
    }

    fishduino_settings_t copy = s_settings;
    bool ok = fn(&copy, ctx);
    if (ok) {
        s_settings = copy;
        if (persist) {
            if (!fishduino_settings_save(&s_settings)) {
                ESP_LOGW(TAG, "NVS save failed after update");
            }
        }
    }

    xSemaphoreGive(s_mutex);
    return ok;
}

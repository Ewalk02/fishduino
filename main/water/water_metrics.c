#include "water_metrics.h"

#include <string.h>

#include "esp_log.h"
#include "maint_tracker/maint_tracker.h"
#include "scheduler/scheduler.h"
#include "water/water_storage.h"

static const char *TAG = "water_metrics";

esp_err_t water_metrics_init(void)
{
    esp_err_t err = water_storage_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "storage init failed");
        return err;
    }
    ESP_LOGI(TAG, "Water metrics initialized");
    return ESP_OK;
}

esp_err_t water_metrics_add_entry(const water_test_entry_t *entry)
{
    if (entry == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    water_test_entry_t copy = *entry;
    copy.valid_flags |= WATER_FLAG_MANUAL;

    if (copy.timestamp_unix == 0) {
        fishduino_time_snapshot_t now;
        fishduino_time_snapshot_now(&now);
        if (now.valid_time) {
            copy.timestamp_unix = (int64_t)now.epoch_seconds;
        } else {
            copy.valid_flags |= WATER_FLAG_TIME_UNKNOWN;
        }
    }

    esp_err_t err = water_storage_append(&copy);
    if (err != ESP_OK) {
        return err;
    }

    maintenance_tracker_mark_done(MAINT_TASK_WATER_TEST);
    ESP_LOGI(TAG, "Water test entry saved");
    return ESP_OK;
}

esp_err_t water_metrics_get_latest(water_test_entry_t *out)
{
    return water_storage_get_latest(out);
}

size_t water_metrics_count(void)
{
    return water_storage_count();
}

esp_err_t water_metrics_get_entries(water_test_entry_t *out, size_t max_entries, size_t *out_count)
{
    return water_storage_get_entries(out, max_entries, out_count);
}

esp_err_t water_metrics_clear_all(void)
{
    return water_storage_clear_all();
}

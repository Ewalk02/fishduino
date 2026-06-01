#include "maintenance_mode.h"

#include <string.h>

#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "scheduler/scheduler.h"
#include "shelly/shelly_manager.h"

static const char *TAG = "maint";
static const char *NVS_NS = "fishduino";
static const char *NVS_KEY_END_EPOCH = "maint_end";

static bool s_active;
static int64_t s_end_epoch;

static int64_t current_epoch(void)
{
    fishduino_time_snapshot_t snap;
    fishduino_time_snapshot_now(&snap);
    if (!snap.valid_time) {
        return 0;
    }
    return (int64_t)snap.epoch_seconds;
}

static void persist_end_epoch(int64_t end_epoch)
{
    nvs_handle_t nvh;
    if (nvs_open(NVS_NS, NVS_READWRITE, &nvh) != ESP_OK) {
        return;
    }
    if (end_epoch > 0) {
        nvs_set_i64(nvh, NVS_KEY_END_EPOCH, end_epoch);
    } else {
        nvs_erase_key(nvh, NVS_KEY_END_EPOCH);
    }
    nvs_commit(nvh);
    nvs_close(nvh);
}

static void load_persisted(void)
{
    nvs_handle_t nvh;
    if (nvs_open(NVS_NS, NVS_READONLY, &nvh) != ESP_OK) {
        return;
    }
    int64_t end = 0;
    if (nvs_get_i64(nvh, NVS_KEY_END_EPOCH, &end) == ESP_OK && end > 0) {
        s_end_epoch = end;
        s_active = true;
    }
    nvs_close(nvh);
}

void fishduino_maintenance_mode_init(void)
{
    s_active = false;
    s_end_epoch = 0;
    load_persisted();
}

void fishduino_maintenance_mode_tick(void)
{
    if (!s_active) {
        return;
    }

    int64_t now = current_epoch();
    if (now > 0 && s_end_epoch > 0 && now >= s_end_epoch) {
        ESP_LOGI(TAG, "Maintenance mode expired");
        fishduino_maintenance_mode_end();
    }
}

bool fishduino_maintenance_mode_is_active(void)
{
    fishduino_maintenance_mode_tick();
    return s_active;
}

int64_t fishduino_maintenance_mode_remaining_ms(void)
{
    if (!s_active || s_end_epoch <= 0) {
        return 0;
    }
    int64_t now = current_epoch();
    if (now <= 0) {
        return 0;
    }
    int64_t sec_left = s_end_epoch - now;
    if (sec_left <= 0) {
        return 0;
    }
    return sec_left * 1000LL;
}

bool fishduino_maintenance_mode_suppress_filter_alarms(void)
{
    return fishduino_maintenance_mode_is_active();
}

esp_err_t fishduino_maintenance_mode_start(uint32_t duration_minutes)
{
    if (duration_minutes == 0) {
        duration_minutes = 30;
    }

    int64_t now = current_epoch();
    if (now <= 0) {
        ESP_LOGW(TAG, "Starting maintenance without valid time; using duration only in RAM");
        s_active = true;
        s_end_epoch = 0;
    } else {
        s_end_epoch = now + (int64_t)duration_minutes * 60LL;
        s_active = true;
        persist_end_epoch(s_end_epoch);
    }

    fishduino_shelly_co2_command_now(false);
    fishduino_shelly_heater_command_now(false);
    ESP_LOGI(TAG, "Maintenance mode started (%u min)", (unsigned)duration_minutes);
    return ESP_OK;
}

esp_err_t fishduino_maintenance_mode_end(void)
{
    s_active = false;
    s_end_epoch = 0;
    persist_end_epoch(0);
    ESP_LOGI(TAG, "Maintenance mode ended");
    return ESP_OK;
}

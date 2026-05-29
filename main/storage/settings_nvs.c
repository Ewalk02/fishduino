#include "settings_nvs.h"

#include <string.h>

#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "shelly/shelly_config.h"

static const char *TAG = "settings";
static const char *NVS_NAMESPACE = "fishduino";
static const char *NVS_KEY_SETTINGS_V2 = "settings_v2";

static void shelly_plug_defaults(fishduino_shelly_plug_settings_t *plug, const char *ip)
{
    memset(plug, 0, sizeof(*plug));
    plug->enabled = true;
    plug->switch_id = 0;
    strncpy(plug->ip, ip, sizeof(plug->ip) - 1);
}

void fishduino_settings_defaults(fishduino_settings_t *out)
{
    memset(out, 0, sizeof(*out));

    out->co2.enabled = false;
    out->co2.on_min = 9 * 60;
    out->co2.off_min = 17 * 60;
    out->co2.manual_override = false;
    out->co2.manual_on = false;

    out->feeder.feed_min_1 = 8 * 60;
    out->feeder.feed_min_2 = 18 * 60;
    out->feeder.pulse_ms = 800;

    shelly_plug_defaults(&out->shelly_co2, FISHDUINO_CO2_SHELLY_IP_DEFAULT);
    shelly_plug_defaults(&out->shelly_filter, FISHDUINO_FILTER_SHELLY_IP_DEFAULT);

    out->filter_running_watts_threshold = 5.0f;
    out->filter_low_power_alarm_delay_s = 60;
    out->co2_command_min_interval_s = 10;
    out->timezone = FISHDUINO_TZ_US_CENTRAL;
}

const char *fishduino_timezone_name(fishduino_timezone_t tz)
{
    switch (tz) {
    case FISHDUINO_TZ_US_EASTERN:
        return "US Eastern";
    case FISHDUINO_TZ_US_CENTRAL:
        return "US Central";
    case FISHDUINO_TZ_US_MOUNTAIN:
        return "US Mountain";
    case FISHDUINO_TZ_US_PACIFIC:
        return "US Pacific";
    default:
        return "Unknown";
    }
}

static esp_err_t ensure_nvs_ready(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "Erasing NVS (no free pages or new version)");
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    return err;
}

bool fishduino_settings_load(fishduino_settings_t *out)
{
    fishduino_settings_defaults(out);

    esp_err_t err = ensure_nvs_ready();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nvs_flash_init failed: %s", esp_err_to_name(err));
        return false;
    }

    nvs_handle_t nvh;
    err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvh);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "No settings yet (nvs_open): %s", esp_err_to_name(err));
        return false;
    }

    size_t size = sizeof(*out);
    err = nvs_get_blob(nvh, NVS_KEY_SETTINGS_V2, out, &size);
    nvs_close(nvh);
    if (err != ESP_OK || size != sizeof(*out)) {
        ESP_LOGW(TAG, "Settings v2 missing or wrong size (%u): %s", (unsigned)size, esp_err_to_name(err));
        fishduino_settings_defaults(out);
        return false;
    }

    return true;
}

bool fishduino_settings_save(const fishduino_settings_t *in)
{
    esp_err_t err = ensure_nvs_ready();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nvs_flash_init failed: %s", esp_err_to_name(err));
        return false;
    }

    nvs_handle_t nvh;
    err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvh);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nvs_open failed: %s", esp_err_to_name(err));
        return false;
    }

    err = nvs_set_blob(nvh, NVS_KEY_SETTINGS_V2, in, sizeof(*in));
    if (err == ESP_OK) {
        err = nvs_commit(nvh);
    }
    nvs_close(nvh);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Save failed: %s", esp_err_to_name(err));
        return false;
    }

    return true;
}

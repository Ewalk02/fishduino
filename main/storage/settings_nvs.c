#include "settings_nvs.h"

#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "shelly/shelly_config.h"
#include "sdkconfig.h"

static const char *TAG = "settings";
static const char *NVS_NAMESPACE = "fishduino";
static const char *NVS_KEY_SETTINGS_V2 = "settings_v2";
static const char *NVS_KEY_SETTINGS_V3 = "settings_v3";
static const char *NVS_KEY_SETTINGS_V4 = "settings_v4";
static const char *NVS_KEY_SETTINGS_V5 = "settings_v5";
static const char *NVS_KEY_SETTINGS_V6 = "settings_v6";
static const char *NVS_KEY_SETTINGS_V7 = "settings_v7";

/** Settings blob before shelly_heater plug (v6). */
typedef struct {
    fishduino_co2_settings_t co2;
    fishduino_feeder_settings_t feeder;
    fishduino_shelly_plug_settings_t shelly_co2;
    fishduino_shelly_plug_settings_t shelly_filter;
    float filter_running_watts_threshold;
    uint16_t filter_low_power_alarm_delay_s;
    uint16_t co2_command_min_interval_s;
    fishduino_timezone_t timezone;
    float filter_baseline_watts;
    fishduino_fluval_settings_t fluval;
    fishduino_heater_settings_t heater;
} fishduino_settings_v6_t;

/** Settings before heater block (v5). */
typedef struct {
    fishduino_co2_settings_t co2;
    fishduino_feeder_settings_t feeder;
    fishduino_shelly_plug_settings_t shelly_co2;
    fishduino_shelly_plug_settings_t shelly_filter;
    float filter_running_watts_threshold;
    uint16_t filter_low_power_alarm_delay_s;
    uint16_t co2_command_min_interval_s;
    fishduino_timezone_t timezone;
    float filter_baseline_watts;
    fishduino_fluval_settings_t fluval;
} fishduino_settings_v5_t;

/** Fluval settings before transport_mode was added (settings v4). */
typedef struct {
    bool enabled;
    char target_name[FISHDUINO_FLUVAL_NAME_LEN];
    char target_mac[FISHDUINO_FLUVAL_MAC_LEN];
    uint16_t poll_interval_s;
    uint16_t stale_timeout_s;
    fishduino_fluval_recipe_t manual_recipe;
} fishduino_fluval_settings_v4_t;

/** Full settings blob before transport_mode was added. */
typedef struct {
    fishduino_co2_settings_t co2;
    fishduino_feeder_settings_t feeder;
    fishduino_shelly_plug_settings_t shelly_co2;
    fishduino_shelly_plug_settings_t shelly_filter;
    float filter_running_watts_threshold;
    uint16_t filter_low_power_alarm_delay_s;
    uint16_t co2_command_min_interval_s;
    fishduino_timezone_t timezone;
    float filter_baseline_watts;
    fishduino_fluval_settings_v4_t fluval;
} fishduino_settings_v4_t;

/** Layout before fluval settings were added. */
typedef struct {
    fishduino_co2_settings_t co2;
    fishduino_feeder_settings_t feeder;
    fishduino_shelly_plug_settings_t shelly_co2;
    fishduino_shelly_plug_settings_t shelly_filter;
    float filter_running_watts_threshold;
    uint16_t filter_low_power_alarm_delay_s;
    uint16_t co2_command_min_interval_s;
    fishduino_timezone_t timezone;
    float filter_baseline_watts;
} fishduino_settings_v3_t;

/** Layout before filter_baseline_watts was added. */
typedef struct {
    fishduino_co2_settings_t co2;
    fishduino_feeder_settings_t feeder;
    fishduino_shelly_plug_settings_t shelly_co2;
    fishduino_shelly_plug_settings_t shelly_filter;
    float filter_running_watts_threshold;
    uint16_t filter_low_power_alarm_delay_s;
    uint16_t co2_command_min_interval_s;
    fishduino_timezone_t timezone;
} fishduino_settings_v2_t;

static void shelly_plug_defaults(fishduino_shelly_plug_settings_t *plug, const char *ip)
{
    memset(plug, 0, sizeof(*plug));
    plug->enabled = true;
    plug->switch_id = 0;
    strncpy(plug->ip, ip, sizeof(plug->ip) - 1);
}

static void heater_defaults(fishduino_heater_settings_t *heater)
{
    memset(heater, 0, sizeof(*heater));
    heater->enabled = false;
    snprintf(heater->name_prefix, sizeof(heater->name_prefix), "DYH1");
    heater->target_temp_f = 77.0f;
    heater->min_temp_f = 50.0f;
    heater->max_temp_f = 95.0f;
    heater->max_over_target_f = 3.0f;
    heater->stale_timeout_s = 30;
}

static void fluval_defaults(fishduino_fluval_settings_t *fluval)
{
    memset(fluval, 0, sizeof(*fluval));
    fluval->enabled = false;
#if CONFIG_FISHDUINO_FLUVAL_DEFAULT_TRANSPORT_HOSTED_BLE
    fluval->transport_mode = FISHDUINO_FLUVAL_TRANSPORT_HOSTED_BLE;
#elif CONFIG_FISHDUINO_FLUVAL_DEFAULT_TRANSPORT_UART
    fluval->transport_mode = FISHDUINO_FLUVAL_TRANSPORT_UART;
#else
    fluval->transport_mode = FISHDUINO_FLUVAL_TRANSPORT_DISABLED;
#endif
    snprintf(fluval->target_name, sizeof(fluval->target_name), "%s", "Plant4.0_450467");
    snprintf(fluval->target_mac, sizeof(fluval->target_mac), "%s", "FA:9D:02:45:04:67");
    fluval->poll_interval_s = 10;
    fluval->stale_timeout_s = 30;
    fluval->manual_recipe.pink = 40;
    fluval->manual_recipe.blue = 20;
    fluval->manual_recipe.cold_white = 60;
    fluval->manual_recipe.white = 70;
    fluval->manual_recipe.warm_white = 50;
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
    shelly_plug_defaults(&out->shelly_heater, FISHDUINO_HEATER_SHELLY_IP_DEFAULT);
    out->shelly_heater.enabled = false;

    out->filter_running_watts_threshold = 5.0f;
    out->filter_baseline_watts = 0.0f;
    out->filter_low_power_alarm_delay_s = 60;
    out->co2_command_min_interval_s = 10;
    out->timezone = FISHDUINO_TZ_US_CENTRAL;
    fluval_defaults(&out->fluval);
    heater_defaults(&out->heater);
}

static void copy_v6_to_v7(const fishduino_settings_v6_t *v6, fishduino_settings_t *out)
{
    fishduino_settings_defaults(out);
    out->co2 = v6->co2;
    out->feeder = v6->feeder;
    out->shelly_co2 = v6->shelly_co2;
    out->shelly_filter = v6->shelly_filter;
    out->filter_running_watts_threshold = v6->filter_running_watts_threshold;
    out->filter_low_power_alarm_delay_s = v6->filter_low_power_alarm_delay_s;
    out->co2_command_min_interval_s = v6->co2_command_min_interval_s;
    out->timezone = v6->timezone;
    out->filter_baseline_watts = v6->filter_baseline_watts;
    out->fluval = v6->fluval;
    out->heater = v6->heater;
}

static void copy_v5_to_v6(const fishduino_settings_v5_t *v5, fishduino_settings_t *out)
{
    fishduino_settings_defaults(out);
    out->co2 = v5->co2;
    out->feeder = v5->feeder;
    out->shelly_co2 = v5->shelly_co2;
    out->shelly_filter = v5->shelly_filter;
    out->filter_running_watts_threshold = v5->filter_running_watts_threshold;
    out->filter_low_power_alarm_delay_s = v5->filter_low_power_alarm_delay_s;
    out->co2_command_min_interval_s = v5->co2_command_min_interval_s;
    out->timezone = v5->timezone;
    out->filter_baseline_watts = v5->filter_baseline_watts;
    out->fluval = v5->fluval;
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

static void copy_v2_to_v3(const fishduino_settings_v2_t *v2, fishduino_settings_t *out)
{
    fishduino_settings_defaults(out);
    out->co2 = v2->co2;
    out->feeder = v2->feeder;
    out->shelly_co2 = v2->shelly_co2;
    out->shelly_filter = v2->shelly_filter;
    out->filter_running_watts_threshold = v2->filter_running_watts_threshold;
    out->filter_low_power_alarm_delay_s = v2->filter_low_power_alarm_delay_s;
    out->co2_command_min_interval_s = v2->co2_command_min_interval_s;
    out->timezone = v2->timezone;
    out->filter_baseline_watts = 0.0f;
}

static void copy_v3_to_v4(const fishduino_settings_v3_t *v3, fishduino_settings_t *out)
{
    fishduino_settings_defaults(out);
    out->co2 = v3->co2;
    out->feeder = v3->feeder;
    out->shelly_co2 = v3->shelly_co2;
    out->shelly_filter = v3->shelly_filter;
    out->filter_running_watts_threshold = v3->filter_running_watts_threshold;
    out->filter_low_power_alarm_delay_s = v3->filter_low_power_alarm_delay_s;
    out->co2_command_min_interval_s = v3->co2_command_min_interval_s;
    out->timezone = v3->timezone;
    out->filter_baseline_watts = v3->filter_baseline_watts;
}

static void copy_v4_to_v5(const fishduino_settings_v4_t *v4, fishduino_settings_t *out)
{
    fishduino_settings_defaults(out);
    out->co2 = v4->co2;
    out->feeder = v4->feeder;
    out->shelly_co2 = v4->shelly_co2;
    out->shelly_filter = v4->shelly_filter;
    out->filter_running_watts_threshold = v4->filter_running_watts_threshold;
    out->filter_low_power_alarm_delay_s = v4->filter_low_power_alarm_delay_s;
    out->co2_command_min_interval_s = v4->co2_command_min_interval_s;
    out->timezone = v4->timezone;
    out->filter_baseline_watts = v4->filter_baseline_watts;

    out->fluval.enabled = v4->fluval.enabled;
    strncpy(out->fluval.target_name, v4->fluval.target_name, sizeof(out->fluval.target_name) - 1);
    strncpy(out->fluval.target_mac, v4->fluval.target_mac, sizeof(out->fluval.target_mac) - 1);
    out->fluval.poll_interval_s = v4->fluval.poll_interval_s;
    out->fluval.stale_timeout_s = v4->fluval.stale_timeout_s;
    out->fluval.manual_recipe = v4->fluval.manual_recipe;
#if CONFIG_FISHDUINO_FLUVAL_DEFAULT_TRANSPORT_HOSTED_BLE
    out->fluval.transport_mode = FISHDUINO_FLUVAL_TRANSPORT_HOSTED_BLE;
#elif CONFIG_FISHDUINO_FLUVAL_DEFAULT_TRANSPORT_UART
    out->fluval.transport_mode = FISHDUINO_FLUVAL_TRANSPORT_UART;
#else
    out->fluval.transport_mode = FISHDUINO_FLUVAL_TRANSPORT_DISABLED;
#endif
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
    err = nvs_get_blob(nvh, NVS_KEY_SETTINGS_V7, out, &size);
    if (err == ESP_OK && size == sizeof(*out)) {
        nvs_close(nvh);
        return true;
    }

    fishduino_settings_v6_t v6 = {0};
    size = sizeof(v6);
    err = nvs_get_blob(nvh, NVS_KEY_SETTINGS_V6, &v6, &size);
    if (err == ESP_OK && size == sizeof(v6)) {
        ESP_LOGI(TAG, "Migrating settings v6 -> v7");
        copy_v6_to_v7(&v6, out);
        nvs_close(nvh);
        fishduino_settings_save(out);
        return true;
    }

    fishduino_settings_v5_t v5 = {0};
    size = sizeof(v5);
    err = nvs_get_blob(nvh, NVS_KEY_SETTINGS_V5, &v5, &size);
    if (err == ESP_OK && size == sizeof(v5)) {
        ESP_LOGI(TAG, "Migrating settings v5 -> v7");
        copy_v5_to_v6(&v5, out);
        nvs_close(nvh);
        fishduino_settings_save(out);
        return true;
    }

    fishduino_settings_v4_t v4 = {0};
    size = sizeof(v4);
    err = nvs_get_blob(nvh, NVS_KEY_SETTINGS_V4, &v4, &size);
    if (err == ESP_OK && size == sizeof(v4)) {
        ESP_LOGI(TAG, "Migrating settings v4 -> v7");
        copy_v4_to_v5(&v4, out);
        nvs_close(nvh);
        fishduino_settings_save(out);
        return true;
    }

    fishduino_settings_v3_t v3 = {0};
    size = sizeof(v3);
    err = nvs_get_blob(nvh, NVS_KEY_SETTINGS_V3, &v3, &size);
    if (err == ESP_OK && size == sizeof(v3)) {
        ESP_LOGI(TAG, "Migrating settings v3 -> v7");
        copy_v3_to_v4(&v3, out);
        nvs_close(nvh);
        fishduino_settings_save(out);
        return true;
    }

    fishduino_settings_v2_t v2 = {0};
    size = sizeof(v2);
    err = nvs_get_blob(nvh, NVS_KEY_SETTINGS_V2, &v2, &size);
    nvs_close(nvh);

    if (err == ESP_OK && size == sizeof(v2)) {
        ESP_LOGI(TAG, "Migrating settings v2 -> v7");
        copy_v2_to_v3(&v2, out);
        fishduino_settings_save(out);
        return true;
    }

    ESP_LOGW(TAG, "Settings missing or wrong size: %s", esp_err_to_name(err));
    fishduino_settings_defaults(out);
    return false;
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

    err = nvs_set_blob(nvh, NVS_KEY_SETTINGS_V7, in, sizeof(*in));
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

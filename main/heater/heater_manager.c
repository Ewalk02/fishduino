#include "heater_manager.h"

#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "esp_timer.h"
#include "heater/chihiros_ble_client.h"
#include "heater/heater_safety.h"
#include "maintenance/maintenance_mode.h"
#include "shelly/shelly_manager.h"
#include "storage/settings_runtime.h"

static const char *TAG = "heater_mgr";

static heater_status_t s_status;
static uint32_t s_last_apply_ms;

static uint32_t now_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000ULL);
}

const char *heater_manager_state_text(heater_state_t state)
{
    switch (state) {
    case HEATER_STATE_OFFLINE:
        return "OFFLINE";
    case HEATER_STATE_CONNECTING:
        return "CONNECTING";
    case HEATER_STATE_ON:
        return "ON";
    case HEATER_STATE_OFF:
        return "OFF";
    case HEATER_STATE_ERROR:
        return "ERROR";
    default:
        return "UNKNOWN";
    }
}

const char *heater_manager_alarm_text(heater_alarm_t alarm)
{
    switch (alarm) {
    case HEATER_ALARM_OFFLINE:
        return "HEATER OFFLINE";
    case HEATER_ALARM_STALE:
        return "HEATER STALE";
    case HEATER_ALARM_OVER_TEMP:
        return "HEATER OVER TEMP";
    case HEATER_ALARM_UNDER_TEMP:
        return "HEATER UNDER TEMP";
    case HEATER_ALARM_COMMAND_FAILED:
        return "HEATER CMD FAILED";
    default:
        return "";
    }
}

static void apply_config_from_settings(void)
{
    fishduino_settings_t st;
    if (!fishduino_settings_get_snapshot(&st)) {
        return;
    }

    chihiros_ble_client_config_t cfg = {0};
    strncpy(cfg.name_prefix, st.heater.name_prefix, sizeof(cfg.name_prefix) - 1);
    cfg.stale_timeout_ms = st.heater.stale_timeout_s > 0 ? (uint32_t)st.heater.stale_timeout_s * 1000U : 10000;
    cfg.min_setpoint_f = st.heater.min_temp_f;
    cfg.max_setpoint_f = st.heater.max_temp_f;
    chihiros_ble_client_set_config(&cfg);
    chihiros_ble_client_set_enabled(st.heater.enabled);
}

esp_err_t heater_manager_init(void)
{
    memset(&s_status, 0, sizeof(s_status));
    s_status.target_temp_f = 77.0f;

    esp_err_t err = chihiros_ble_client_init();
    if (err != ESP_OK) {
        return err;
    }

    apply_config_from_settings();
    ESP_LOGI(TAG, "Heater manager initialized");
    return ESP_OK;
}

static void refresh_status_from_ble(void)
{
    fishduino_settings_t st;
    fishduino_settings_get_snapshot(&st);

    chihiros_ble_client_state_t ble;
    chihiros_ble_client_get_state(&ble);

    s_status.enabled = st.heater.enabled;
    s_status.target_temp_f = st.heater.target_temp_f;
    s_status.online = ble.connected && ble.subscribed;
    s_status.stale = ble.stale || !ble.last_status.valid;
    s_status.heating = ble.last_status.valid && ble.last_status.heating;
    s_status.rssi = 0;

    if (ble.last_status.valid) {
        s_status.reported_temp_f = ble.last_status.current_temp_f;
        s_status.last_seen_ms = (int64_t)ble.last_status_ms;
    } else {
        s_status.reported_temp_f = 0.0f;
    }

    if (!st.heater.enabled) {
        s_status.state = HEATER_STATE_OFF;
        s_status.alarm = HEATER_ALARM_NONE;
        snprintf(s_status.error_text, sizeof(s_status.error_text), "disabled");
        return;
    }

    if (!s_status.online) {
        s_status.state = HEATER_STATE_OFFLINE;
        s_status.alarm = HEATER_ALARM_OFFLINE;
        snprintf(s_status.error_text, sizeof(s_status.error_text), "offline");
        return;
    }

    if (s_status.stale) {
        s_status.state = HEATER_STATE_ERROR;
        s_status.alarm = HEATER_ALARM_STALE;
        snprintf(s_status.error_text, sizeof(s_status.error_text), "stale");
        return;
    }

    if (s_status.reported_temp_f > st.heater.target_temp_f + st.heater.max_over_target_f) {
        s_status.state = HEATER_STATE_ERROR;
        s_status.alarm = HEATER_ALARM_OVER_TEMP;
        snprintf(s_status.error_text, sizeof(s_status.error_text), "over temp");
        return;
    }

    s_status.alarm = HEATER_ALARM_NONE;
    s_status.error_text[0] = '\0';
    s_status.state = s_status.heating ? HEATER_STATE_ON : HEATER_STATE_OFF;
}

static bool heater_shelly_power_allowed(const fishduino_settings_t *st)
{
    if (st == NULL || !st->shelly_heater.enabled) {
        return false;
    }
    if (!st->heater.enabled) {
        return false;
    }
    if (fishduino_maintenance_mode_is_active()) {
        return false;
    }
    if (s_status.alarm != HEATER_ALARM_NONE) {
        return false;
    }

    char err[64] = {0};
    if (!heater_safety_allows_heating(&s_status, st->heater.target_temp_f, err, sizeof(err))) {
        return false;
    }
    return true;
}

void heater_manager_tick(void)
{
    apply_config_from_settings();
    refresh_status_from_ble();

    fishduino_settings_t st;
    if (!fishduino_settings_get_snapshot(&st)) {
        return;
    }

    fishduino_shelly_heater_apply_power(heater_shelly_power_allowed(&st));

    if (!st.heater.enabled) {
        return;
    }

    char err[64] = {0};
    bool allow = heater_safety_allows_heating(&s_status, st.heater.target_temp_f, err, sizeof(err));

    uint32_t t = now_ms();
    if (allow) {
        if (s_last_apply_ms == 0 || (t - s_last_apply_ms) > 30000) {
            if (chihiros_ble_client_set_setpoint_f(st.heater.target_temp_f)) {
                s_last_apply_ms = t;
            } else {
                s_status.alarm = HEATER_ALARM_COMMAND_FAILED;
                snprintf(s_status.error_text, sizeof(s_status.error_text), "setpoint failed");
            }
        }
    } else if (s_status.online && !s_status.stale) {
        if (s_last_apply_ms == 0 || (t - s_last_apply_ms) > 60000) {
            chihiros_ble_client_set_setpoint_f(st.heater.min_temp_f);
            s_last_apply_ms = t;
        }
        if (err[0]) {
            snprintf(s_status.error_text, sizeof(s_status.error_text), "%s", err);
        }
    }
}

static bool mutator_heater_enabled(fishduino_settings_t *st, void *ctx)
{
    st->heater.enabled = (bool)(intptr_t)ctx;
    return true;
}

static bool mutator_heater_target(fishduino_settings_t *st, void *ctx)
{
    float target_f = *(float *)ctx;
    if (target_f < st->heater.min_temp_f) {
        target_f = st->heater.min_temp_f;
    }
    if (target_f > st->heater.max_temp_f) {
        target_f = st->heater.max_temp_f;
    }
    st->heater.target_temp_f = target_f;
    return true;
}

esp_err_t heater_manager_set_enabled(bool enabled)
{
    if (!fishduino_settings_update(mutator_heater_enabled, (void *)(intptr_t)enabled, true)) {
        return ESP_FAIL;
    }
    apply_config_from_settings();
    return ESP_OK;
}

esp_err_t heater_manager_set_target_temp_f(float target_f)
{
    if (!fishduino_settings_update(mutator_heater_target, &target_f, true)) {
        return ESP_FAIL;
    }
    s_last_apply_ms = 0;
    return ESP_OK;
}

esp_err_t heater_manager_get_status(heater_status_t *out)
{
    if (out == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    refresh_status_from_ble();
    *out = s_status;
    return ESP_OK;
}

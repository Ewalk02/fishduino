#include "co2_safety.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "maintenance/maintenance_mode.h"
#include "net/wifi_manager.h"
#include "scheduler/scheduler.h"
#include "shelly/shelly_manager.h"
#include "storage/settings_runtime.h"
#include "net/wifi_manager.h"

#define CO2_FILTER_STALE_MULT 2
#define CO2_FILTER_STALE_MIN_MS 15000
#define CO2_FILTER_ON_HYSTERESIS_MS 5000

static co2_safety_reason_t s_last_block = CO2_BLOCK_NONE;
static int64_t s_override_end_ms = 0;

static uint32_t now_ms(void)
{
    return (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
}

void fishduino_co2_safety_init(void)
{
    s_last_block = CO2_BLOCK_NONE;
    s_override_end_ms = 0;
}

const char *fishduino_co2_safety_reason_text(co2_safety_reason_t reason)
{
    switch (reason) {
    case CO2_BLOCK_NONE:
        return "OK";
    case CO2_BLOCK_MAINTENANCE:
        return "maintenance mode";
    case CO2_BLOCK_CO2_OFFLINE:
        return "CO2 plug offline";
    case CO2_BLOCK_FILTER_DISABLED:
        return "filter monitor disabled";
    case CO2_BLOCK_FILTER_OFFLINE:
        return "filter offline";
    case CO2_BLOCK_FILTER_STALE:
        return "filter status stale";
    case CO2_BLOCK_FILTER_UNCALIBRATED:
        return "filter not calibrated";
    case CO2_BLOCK_FILTER_OFF:
        return "filter off";
    case CO2_BLOCK_FILTER_LOW_POWER:
        return "filter power low";
    case CO2_BLOCK_FILTER_BELOW_THRESHOLD:
        return "filter below running threshold";
    case CO2_BLOCK_NO_TIME_SYNC:
        return "waiting for time sync";
    case CO2_BLOCK_GLOBAL_LOCKOUT:
        return "safety lockout";
    case CO2_BLOCK_CO2_COMMAND_FAILED:
        return "CO2 command failed";
    default:
        return "blocked";
    }
}

void fishduino_co2_safety_set_last_block(co2_safety_reason_t reason)
{
    s_last_block = reason;
}

co2_safety_reason_t fishduino_co2_safety_get_last_block(void)
{
    return s_last_block;
}

bool fishduino_co2_safety_override_active(void)
{
    if (s_override_end_ms <= 0) {
        return false;
    }
    int64_t now = (int64_t)now_ms();
    if (now >= s_override_end_ms) {
        s_override_end_ms = 0;
        return false;
    }
    return true;
}

void fishduino_co2_safety_override_start(uint32_t duration_minutes)
{
    if (duration_minutes == 0) {
        duration_minutes = 15;
    }
    s_override_end_ms = (int64_t)now_ms() + (int64_t)duration_minutes * 60000LL;
}

void fishduino_co2_safety_override_clear(void)
{
    s_override_end_ms = 0;
}

static bool filter_status_stale(const fishduino_shelly_state_t *ss)
{
    const fishduino_shelly_switch_status_t *fs = &ss->filter_status;
    if (!fs->online || fs->last_success_ms == 0) {
        return true;
    }
    uint32_t age = now_ms() - fs->last_success_ms;
    uint32_t limit = CO2_FILTER_STALE_MIN_MS;
    if (limit < 5000) {
        limit = 5000;
    }
    return age > limit;
}

static bool filter_running_ok(const fishduino_settings_t *settings, const fishduino_shelly_state_t *ss)
{
    const fishduino_shelly_switch_status_t *fs = &ss->filter_status;

    if (!fs->online || !fs->output) {
        return false;
    }

    if (fs->watts >= settings->filter_running_watts_threshold) {
        return true;
    }

    if (ss->last_filter_good_power_ms == 0) {
        return false;
    }

    uint32_t elapsed = now_ms() - ss->last_filter_good_power_ms;
    return elapsed < CO2_FILTER_ON_HYSTERESIS_MS;
}

bool fishduino_co2_safety_allows_on(co2_safety_reason_t *reason)
{
    co2_safety_reason_t r = CO2_BLOCK_NONE;

    if (fishduino_maintenance_mode_is_active()) {
        r = CO2_BLOCK_MAINTENANCE;
        goto out;
    }

    if (fishduino_co2_safety_override_active()) {
        r = CO2_BLOCK_NONE;
        goto out;
    }

    fishduino_settings_t settings;
    fishduino_shelly_state_t ss;
    if (!fishduino_settings_get_snapshot(&settings) || !fishduino_shelly_manager_get_state_snapshot(&ss)) {
        r = CO2_BLOCK_GLOBAL_LOCKOUT;
        goto out;
    }

    if (!settings.shelly_co2.enabled) {
        r = CO2_BLOCK_NONE;
        goto out;
    }

    if (!fishduino_wifi_is_connected() || !ss.co2_status.online) {
        r = CO2_BLOCK_CO2_OFFLINE;
        goto out;
    }

    if (!settings.shelly_filter.enabled) {
        r = CO2_BLOCK_FILTER_DISABLED;
        goto out;
    }

    if (settings.filter_baseline_watts <= 0.0f) {
        r = CO2_BLOCK_FILTER_UNCALIBRATED;
        goto out;
    }

    if (!fishduino_wifi_is_connected() || !ss.filter_status.online) {
        r = CO2_BLOCK_FILTER_OFFLINE;
        goto out;
    }

    if (filter_status_stale(&ss)) {
        r = CO2_BLOCK_FILTER_STALE;
        goto out;
    }

    if (ss.filter_alarm == FISHDUINO_FILTER_ALARM_OFF) {
        r = CO2_BLOCK_FILTER_OFF;
        goto out;
    }

    if (ss.filter_alarm == FISHDUINO_FILTER_ALARM_LOW_POWER) {
        r = CO2_BLOCK_FILTER_LOW_POWER;
        goto out;
    }

    if (ss.filter_alarm == FISHDUINO_FILTER_ALARM_OFFLINE) {
        r = CO2_BLOCK_FILTER_OFFLINE;
        goto out;
    }

    if (!filter_running_ok(&settings, &ss)) {
        r = CO2_BLOCK_FILTER_BELOW_THRESHOLD;
        goto out;
    }

out:
    if (reason != NULL) {
        *reason = r;
    }
    fishduino_co2_safety_set_last_block(r);
    return r == CO2_BLOCK_NONE;
}

bool fishduino_co2_safety_effective_desired_on(bool wants_on, co2_safety_reason_t *reason)
{
    if (!wants_on) {
        if (reason != NULL) {
            *reason = CO2_BLOCK_NONE;
        }
        fishduino_co2_safety_set_last_block(CO2_BLOCK_NONE);
        return false;
    }

    fishduino_time_snapshot_t now;
    fishduino_time_snapshot_now(&now);
    if (!now.valid_time) {
        if (reason != NULL) {
            *reason = CO2_BLOCK_NO_TIME_SYNC;
        }
        fishduino_co2_safety_set_last_block(CO2_BLOCK_NO_TIME_SYNC);
        return false;
    }

    return fishduino_co2_safety_allows_on(reason);
}

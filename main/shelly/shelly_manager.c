#include "shelly_manager.h"

#include <string.h>

#include "co2/co2_gpio.h"
#include "co2/co2_schedule.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "net/shelly_client.h"
#include "net/time_sync.h"
#include "net/wifi_manager.h"
#include "shelly/shelly_config.h"
#include "maintenance/maintenance_mode.h"
#include "safety/co2_safety.h"
#include "storage/settings_runtime.h"

static const char *TAG = "shelly_mgr";

typedef enum {
    SHELLY_CMD_CO2_SET = 1,
    SHELLY_CMD_FILTER_CALIBRATE,
    SHELLY_CMD_HEATER_SET,
} shelly_cmd_type_t;

typedef struct {
    shelly_cmd_type_t type;
    bool on;
} shelly_cmd_t;

static fishduino_shelly_state_t s_state;
static SemaphoreHandle_t s_state_mutex;
static QueueHandle_t s_cmd_queue;

static uint32_t now_ms(void)
{
    return (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
}

static void state_lock(void)
{
    if (s_state_mutex != NULL) {
        xSemaphoreTake(s_state_mutex, portMAX_DELAY);
    }
}

static void state_unlock(void)
{
    if (s_state_mutex != NULL) {
        xSemaphoreGive(s_state_mutex);
    }
}

bool fishduino_shelly_manager_get_state_snapshot(fishduino_shelly_state_t *out)
{
    if (out == NULL || s_state_mutex == NULL) {
        return false;
    }
    if (xSemaphoreTake(s_state_mutex, pdMS_TO_TICKS(200)) != pdTRUE) {
        return false;
    }
    *out = s_state;
    xSemaphoreGive(s_state_mutex);
    return true;
}

static void snapshot_filter_last_known(void)
{
    s_state.filter_last_known = s_state.filter_status;
    s_state.filter_last_known_age_ms = 0;
}

static void update_filter_last_known_age(void)
{
    if (s_state.filter_last_known.last_success_ms == 0) {
        return;
    }
    uint32_t t = now_ms();
    if (t >= s_state.filter_last_known.last_success_ms) {
        s_state.filter_last_known_age_ms = t - s_state.filter_last_known.last_success_ms;
    }
}

const char *fishduino_filter_alarm_text(fishduino_filter_alarm_t alarm)
{
    switch (alarm) {
    case FISHDUINO_FILTER_ALARM_OFF:
        return "FILTER IS OFF";
    case FISHDUINO_FILTER_ALARM_OFFLINE:
        return "FILTER MONITOR OFFLINE";
    case FISHDUINO_FILTER_ALARM_LOW_POWER:
        return "FILTER POWER LOW";
    default:
        return "OK";
    }
}

static void update_filter_alarm(const fishduino_settings_t *settings)
{
    const fishduino_shelly_switch_status_t *fs = &s_state.filter_status;

    if (!settings->shelly_filter.enabled) {
        s_state.filter_alarm = FISHDUINO_FILTER_ALARM_NONE;
        s_state.filter_output_off_alert = false;
        return;
    }

    if (fishduino_maintenance_mode_suppress_filter_alarms()) {
        s_state.filter_alarm = FISHDUINO_FILTER_ALARM_NONE;
        return;
    }

    if (fs->online && fs->output) {
        s_state.filter_output_off_alert = false;
    } else if (fs->online && !fs->output) {
        s_state.filter_output_off_alert = true;
    }

    if (s_state.filter_output_off_alert) {
        s_state.filter_alarm = FISHDUINO_FILTER_ALARM_OFF;
        return;
    }

    if (!fishduino_wifi_is_connected() || !fs->online) {
        s_state.filter_alarm = FISHDUINO_FILTER_ALARM_OFFLINE;
        update_filter_last_known_age();
        return;
    }

    s_state.filter_alarm = FISHDUINO_FILTER_ALARM_NONE;

    if (fs->output && fs->watts < settings->filter_running_watts_threshold) {
        uint32_t t = now_ms();
        if (s_state.last_filter_good_power_ms == 0) {
            s_state.last_filter_good_power_ms = t;
        }
        uint32_t elapsed = t - s_state.last_filter_good_power_ms;
        if (elapsed >= (uint32_t)settings->filter_low_power_alarm_delay_s * 1000U) {
            s_state.filter_alarm = FISHDUINO_FILTER_ALARM_LOW_POWER;
        }
    } else if (fs->output) {
        s_state.last_filter_good_power_ms = now_ms();
    }
}

static void mark_failure_placeholder(fishduino_shelly_switch_status_t *status)
{
    status->fail_count++;
    if (status->fail_count >= FISHDUINO_SHELLY_FAIL_OFFLINE) {
        status->online = false;
    }
}

/** HTTP poll only — no access to s_state (caller must hold status seeded from prior snapshot). */
static bool poll_plug_http(const fishduino_shelly_plug_settings_t *plug, fishduino_shelly_switch_status_t *status)
{
    if (!plug->enabled || plug->ip[0] == '\0') {
        status->online = false;
        return false;
    }

    if (!fishduino_wifi_is_connected()) {
        mark_failure_placeholder(status);
        return false;
    }

    return fishduino_shelly_get_switch_status(plug->ip, plug->switch_id, status);
}

/** Caller must hold state_lock. */
static void apply_filter_status_locked(const fishduino_shelly_switch_status_t *tmp, bool poll_ok)
{
    s_state.filter_status = *tmp;
    if (poll_ok && tmp->online) {
        snapshot_filter_last_known();
    } else if (!tmp->online) {
        update_filter_last_known_age();
    }
}

static bool co2_set_allowed_interval_ms(uint32_t last_co2_command_ms, const fishduino_settings_t *settings)
{
    uint32_t interval_ms = (uint32_t)settings->co2_command_min_interval_s * 1000U;
    if (interval_ms < FISHDUINO_SHELLY_CO2_CMD_MIN_MS) {
        interval_ms = FISHDUINO_SHELLY_CO2_CMD_MIN_MS;
    }
    uint32_t t = now_ms();
    if (last_co2_command_ms != 0 && (t - last_co2_command_ms) < interval_ms) {
        return false;
    }
    return true;
}

/** HTTP outside state mutex. */
static bool execute_co2_set(const fishduino_settings_t *settings, bool on)
{
    if (!settings->shelly_co2.enabled) {
        return false;
    }

    co2_safety_reason_t reason = CO2_BLOCK_NONE;
    if (on && !fishduino_co2_safety_allows_on(&reason)) {
        ESP_LOGW(TAG, "CO2 ON blocked: %s", fishduino_co2_safety_reason_text(reason));
        on = false;
        state_lock();
        s_state.co2_block_reason = reason;
        state_unlock();
    } else if (on) {
        state_lock();
        s_state.co2_block_reason = CO2_BLOCK_NONE;
        state_unlock();
    }

    uint32_t last_cmd_ms;
    state_lock();
    last_cmd_ms = s_state.last_co2_command_ms;
    state_unlock();

    if (!co2_set_allowed_interval_ms(last_cmd_ms, settings)) {
        return false;
    }

    if (!fishduino_shelly_co2_set_output(settings, on)) {
        if (on) {
            state_lock();
            s_state.co2_block_reason = CO2_BLOCK_CO2_COMMAND_FAILED;
            state_unlock();
        }
        return false;
    }

    state_lock();
    s_state.co2_last_sent_on = on;
    s_state.last_co2_command_ms = now_ms();
    s_state.co2_status.output = on;
    state_unlock();
    return true;
}

static void record_heater_command_attempt(void)
{
    state_lock();
    s_state.last_heater_command_ms = now_ms();
    state_unlock();
}

static bool execute_heater_set(const fishduino_settings_t *settings, bool on)
{
    if (!settings->shelly_heater.enabled) {
        return false;
    }

    record_heater_command_attempt();

    if (!fishduino_shelly_heater_set_output(settings, on)) {
        return false;
    }

    state_lock();
    s_state.heater_status.output = on;
    state_unlock();
    return true;
}

typedef struct {
    float sum_watts;
    unsigned sample_count;
    uint32_t start_ms;
} filter_cal_ctx_t;

static bool s_cal_active;
static uint32_t s_cal_start_ms;
static filter_cal_ctx_t s_cal_acc;

static const uint32_t FILTER_CAL_DURATION_MS = 30000;

static bool mutator_filter_calibrate(fishduino_settings_t *st, void *ctx)
{
    filter_cal_ctx_t *c = (filter_cal_ctx_t *)ctx;
    if (c->sample_count == 0) {
        return false;
    }
    float baseline = c->sum_watts / (float)c->sample_count;
    float threshold = baseline * 0.5f;
    if (threshold < 5.0f) {
        threshold = 5.0f;
    }
    st->filter_baseline_watts = baseline;
    st->filter_running_watts_threshold = threshold;
    ESP_LOGI(TAG, "Filter calibrated: baseline=%.1f W threshold=%.1f W", (double)baseline, (double)threshold);
    return true;
}

static void filter_calibrate_start(const fishduino_settings_t *settings)
{
    if (!settings->shelly_filter.enabled) {
        ESP_LOGW(TAG, "Filter calibrate: filter plug disabled");
        return;
    }
    memset(&s_cal_acc, 0, sizeof(s_cal_acc));
    s_cal_start_ms = now_ms();
    s_cal_active = true;

    state_lock();
    s_state.filter_calibrating = true;
    s_state.filter_calibrate_progress_s = 0;
    state_unlock();

    ESP_LOGI(TAG, "Filter calibration started (non-blocking)");
}

static void filter_calibrate_tick(const fishduino_settings_t *settings)
{
    if (!s_cal_active) {
        return;
    }

    fishduino_shelly_switch_status_t tmp;
    state_lock();
    tmp = s_state.filter_status;
    state_unlock();

    poll_plug_http(&settings->shelly_filter, &tmp);

    state_lock();
    s_state.filter_status = tmp;

    if (tmp.online && tmp.output && tmp.watts >= 0.0f) {
        s_cal_acc.sum_watts += tmp.watts;
        s_cal_acc.sample_count++;
    }

    uint32_t elapsed = now_ms() - s_cal_start_ms;
    uint8_t prog = (uint8_t)(elapsed / 1000U);
    if (prog > 30) {
        prog = 30;
    }
    s_state.filter_calibrate_progress_s = prog;
    state_unlock();

    if (elapsed < FILTER_CAL_DURATION_MS) {
        return;
    }

    s_cal_active = false;
    state_lock();
    s_state.filter_calibrating = false;
    s_state.filter_calibrate_progress_s = 30;
    state_unlock();

    if (s_cal_acc.sample_count > 0) {
        s_cal_acc.start_ms = s_cal_start_ms;
        fishduino_settings_update(mutator_filter_calibrate, &s_cal_acc, true);
    } else {
        ESP_LOGW(TAG, "Filter calibrate: no samples (filter on and online?)");
    }

    memset(&s_cal_acc, 0, sizeof(s_cal_acc));

    state_lock();
    update_filter_alarm(settings);
    state_unlock();
}

static void shelly_task(void *arg)
{
    (void)arg;
    uint32_t last_poll = 0;

    while (true) {
        fishduino_settings_t settings;
        if (!fishduino_settings_get_snapshot(&settings)) {
            vTaskDelay(pdMS_TO_TICKS(200));
            continue;
        }

        shelly_cmd_t cmd;

        while (xQueueReceive(s_cmd_queue, &cmd, 0) == pdTRUE) {
            if (cmd.type == SHELLY_CMD_CO2_SET) {
                fishduino_settings_t co2_settings;
                if (fishduino_settings_get_snapshot(&co2_settings)) {
                    execute_co2_set(&co2_settings, cmd.on);
                }
            } else if (cmd.type == SHELLY_CMD_FILTER_CALIBRATE) {
                filter_calibrate_start(&settings);
            } else if (cmd.type == SHELLY_CMD_HEATER_SET) {
                fishduino_settings_t heater_settings;
                if (fishduino_settings_get_snapshot(&heater_settings)) {
                    execute_heater_set(&heater_settings, cmd.on);
                }
            }
        }

        if (s_cal_active) {
            filter_calibrate_tick(&settings);
        }

        uint32_t t = now_ms();
        if ((t - last_poll) >= FISHDUINO_SHELLY_POLL_MS) {
            last_poll = t;

            fishduino_shelly_switch_status_t co2_tmp;
            fishduino_shelly_switch_status_t filter_tmp;
            fishduino_shelly_switch_status_t heater_tmp;
            bool filter_ok = false;

            state_lock();
            co2_tmp = s_state.co2_status;
            filter_tmp = s_state.filter_status;
            heater_tmp = s_state.heater_status;
            state_unlock();

            if (settings.shelly_co2.enabled) {
                poll_plug_http(&settings.shelly_co2, &co2_tmp);
            }
            if (settings.shelly_filter.enabled && !s_cal_active) {
                filter_ok = poll_plug_http(&settings.shelly_filter, &filter_tmp);
            }
            if (settings.shelly_heater.enabled) {
                poll_plug_http(&settings.shelly_heater, &heater_tmp);
            }

            state_lock();
            s_state.last_poll_ms = t;

            if (settings.shelly_co2.enabled) {
                s_state.co2_status = co2_tmp;
            }
            if (settings.shelly_heater.enabled) {
                s_state.heater_status = heater_tmp;
            }
            if (settings.shelly_filter.enabled && !s_cal_active) {
                bool was_off = !s_state.filter_status.output;
                apply_filter_status_locked(&filter_tmp, filter_ok);
                if (s_state.filter_status.output) {
                    s_state.last_filter_output_on_ms = t;
                    if (was_off) {
                        ESP_LOGW(TAG, "Filter plug output restored");
                    }
                }
            }

            update_filter_alarm(&settings);
            state_unlock();
        } else {
            state_lock();
            if (s_state.filter_alarm == FISHDUINO_FILTER_ALARM_OFFLINE) {
                update_filter_last_known_age();
            }
            state_unlock();
        }

        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

void fishduino_shelly_manager_init(void)
{
    memset(&s_state, 0, sizeof(s_state));
    s_state.co2_manual_last_seen_min = 0xFFFF;

    if (s_state_mutex == NULL) {
        s_state_mutex = xSemaphoreCreateMutex();
    }

    s_cmd_queue = xQueueCreate(8, sizeof(shelly_cmd_t));
    if (s_cmd_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create command queue");
        return;
    }

    if (xTaskCreate(shelly_task, "shelly", 8192, NULL, 4, NULL) != pdPASS) {
        ESP_LOGE(TAG, "Failed to create shelly task");
    } else {
        ESP_LOGI(TAG, "Shelly manager started");
    }
}

static void queue_co2_set(bool on)
{
    if (s_cmd_queue == NULL) {
        return;
    }
    shelly_cmd_t cmd = {.type = SHELLY_CMD_CO2_SET, .on = on};
    xQueueSend(s_cmd_queue, &cmd, 0);
}

static bool mutator_co2_manual(fishduino_settings_t *st, void *ctx)
{
    bool on = (bool)(intptr_t)ctx;
    st->co2.manual_override = true;
    st->co2.manual_on = on;
    return true;
}

static bool mutator_co2_auto(fishduino_settings_t *st, void *ctx)
{
    (void)ctx;
    st->co2.manual_override = false;
    return true;
}

static bool mutator_co2_schedule(fishduino_settings_t *st, void *ctx)
{
    st->co2.enabled = (bool)(intptr_t)ctx;
    return true;
}

static bool mutator_timezone(fishduino_settings_t *st, void *ctx)
{
    fishduino_timezone_t tz = (fishduino_timezone_t)(intptr_t)ctx;
    if (tz > FISHDUINO_TZ_US_PACIFIC) {
        return false;
    }
    st->timezone = tz;
    fishduino_time_sync_apply_timezone(st);
    return true;
}

static bool mutator_clear_manual(fishduino_settings_t *st, void *ctx)
{
    (void)ctx;
    st->co2.manual_override = false;
    return true;
}

static bool crossed_schedule_boundary(uint16_t prev_min, uint16_t now_min, uint16_t boundary)
{
    if (prev_min == now_min) {
        return false;
    }
    if (prev_min < now_min) {
        return prev_min < boundary && now_min >= boundary;
    }
    return prev_min < boundary || now_min >= boundary;
}

static void clear_manual_at_schedule_transition(fishduino_co2_t *co2, const fishduino_time_snapshot_t *now)
{
    if (!co2->settings.co2.manual_override || !now->valid_time) {
        if (now->valid_time) {
            state_lock();
            s_state.co2_manual_last_seen_min = now->minutes_since_midnight;
            state_unlock();
        }
        return;
    }

    uint16_t now_min = now->minutes_since_midnight;
    uint16_t prev_min;

    state_lock();
    prev_min = s_state.co2_manual_last_seen_min;
    state_unlock();

    if (prev_min == 0xFFFF) {
        state_lock();
        s_state.co2_manual_last_seen_min = now_min;
        state_unlock();
        return;
    }

    bool crossed = crossed_schedule_boundary(prev_min, now_min, co2->settings.co2.on_min) ||
                   crossed_schedule_boundary(prev_min, now_min, co2->settings.co2.off_min);

    state_lock();
    s_state.co2_manual_last_seen_min = now_min;
    state_unlock();

    if (crossed) {
        co2->settings.co2.manual_override = false;
        fishduino_settings_update(mutator_clear_manual, NULL, true);
        state_lock();
        s_state.co2_manual_active = false;
        state_unlock();
        ESP_LOGI(TAG, "CO2 manual override cleared at schedule transition");
    }
}

void fishduino_shelly_co2_tick(fishduino_co2_t *co2, const fishduino_time_snapshot_t *now)
{
    fishduino_settings_t settings;
    if (!fishduino_settings_get_snapshot(&settings) || !settings.shelly_co2.enabled) {
        return;
    }

    state_lock();
    s_state.co2_manual_active = co2->settings.co2.manual_override;
    state_unlock();

    clear_manual_at_schedule_transition(co2, now);

    bool target = fishduino_co2_get_target(co2, now);
    co2_safety_reason_t reason = CO2_BLOCK_NONE;
    target = fishduino_co2_safety_effective_desired_on(target, &reason);

    state_lock();
    s_state.co2_desired_on = target;
    s_state.co2_block_reason = reason;
    s_state.co2_waiting_time = !now->valid_time;
    if (!now->valid_time) {
        target = false;
        s_state.co2_desired_on = false;
        s_state.co2_block_reason = CO2_BLOCK_NO_TIME_SYNC;
    }

    if (!fishduino_co2_gpio_is_configured() || settings.shelly_co2.enabled) {
        if (target != s_state.co2_last_sent_on ||
            (s_state.co2_status.online && s_state.co2_status.output != target)) {
            state_unlock();
            queue_co2_set(target);
            return;
        }
    }
    state_unlock();
}

void fishduino_shelly_filter_alarm_tick(void)
{
    fishduino_settings_t settings;
    if (!fishduino_settings_get_snapshot(&settings)) {
        return;
    }
    state_lock();
    s_state.alert_blink_on = !s_state.alert_blink_on;
    update_filter_alarm(&settings);
    state_unlock();
}

void fishduino_shelly_co2_manual(bool on)
{
    fishduino_settings_update(mutator_co2_manual, (void *)(intptr_t)on, true);
    state_lock();
    s_state.co2_manual_active = true;
    s_state.co2_manual_last_seen_min = 0xFFFF;
    state_unlock();
    queue_co2_set(on);
}

void fishduino_shelly_co2_auto(void)
{
    fishduino_settings_update(mutator_co2_auto, NULL, true);
    state_lock();
    s_state.co2_manual_active = false;
    s_state.co2_manual_last_seen_min = 0xFFFF;
    state_unlock();
}

void fishduino_shelly_co2_command_now(bool on)
{
    queue_co2_set(on);
}

static void queue_heater_set(bool on)
{
    if (s_cmd_queue == NULL) {
        return;
    }
    shelly_cmd_t cmd = {.type = SHELLY_CMD_HEATER_SET, .on = on};
    xQueueSend(s_cmd_queue, &cmd, 0);
}

void fishduino_shelly_heater_command_now(bool on)
{
    queue_heater_set(on);
}

static bool heater_cmd_interval_elapsed(uint32_t last_cmd_ms)
{
    uint32_t t = now_ms();
    if (last_cmd_ms == 0) {
        return true;
    }
    return (t - last_cmd_ms) >= FISHDUINO_SHELLY_HEATER_CMD_MIN_MS;
}

void fishduino_shelly_heater_apply_power(bool want_on)
{
    fishduino_settings_t settings;
    if (!fishduino_settings_get_snapshot(&settings) || !settings.shelly_heater.enabled) {
        return;
    }

    bool relay_on = false;
    bool online = false;
    uint32_t last_cmd_ms = 0;
    state_lock();
    relay_on = s_state.heater_status.output;
    online = s_state.heater_status.online;
    last_cmd_ms = s_state.last_heater_command_ms;
    state_unlock();

    if (online && relay_on == want_on) {
        return;
    }

    if (!heater_cmd_interval_elapsed(last_cmd_ms)) {
        return;
    }

    record_heater_command_attempt();
    queue_heater_set(want_on);
}

void fishduino_shelly_set_co2_schedule_enabled(bool enabled)
{
    fishduino_settings_update(mutator_co2_schedule, (void *)(intptr_t)enabled, true);
    ESP_LOGI(TAG, "CO2 schedule %s", enabled ? "enabled" : "disabled");
}

void fishduino_shelly_set_timezone(fishduino_timezone_t tz)
{
    fishduino_settings_update(mutator_timezone, (void *)(intptr_t)tz, true);
}

void fishduino_shelly_filter_calibrate_start(void)
{
    if (s_cmd_queue == NULL) {
        return;
    }
    shelly_cmd_t cmd = {.type = SHELLY_CMD_FILTER_CALIBRATE};
    xQueueSend(s_cmd_queue, &cmd, 0);
}

bool fishduino_shelly_poll_filter_now(void)
{
    fishduino_settings_t settings;
    if (!fishduino_settings_get_snapshot(&settings) || !settings.shelly_filter.enabled) {
        return false;
    }

    fishduino_shelly_switch_status_t tmp;
    state_lock();
    tmp = s_state.filter_status;
    state_unlock();

    bool ok = poll_plug_http(&settings.shelly_filter, &tmp);

    state_lock();
    apply_filter_status_locked(&tmp, ok);
    update_filter_alarm(&settings);
    state_unlock();
    return true;
}

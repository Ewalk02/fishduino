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
#include "storage/settings_runtime.h"

static const char *TAG = "shelly_mgr";

typedef enum {
    SHELLY_CMD_CO2_SET = 1,
    SHELLY_CMD_FILTER_CALIBRATE,
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

const fishduino_shelly_state_t *fishduino_shelly_manager_get_state(void)
{
    return &s_state;
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

static void poll_plug(const fishduino_shelly_plug_settings_t *plug, fishduino_shelly_switch_status_t *status,
                      bool is_filter)
{
    if (!plug->enabled || plug->ip[0] == '\0') {
        status->online = false;
        return;
    }

    if (!fishduino_wifi_is_connected()) {
        mark_failure_placeholder(status);
        if (is_filter) {
            update_filter_last_known_age();
        }
        return;
    }

    if (fishduino_shelly_get_switch_status(plug->ip, plug->switch_id, status) && is_filter) {
        snapshot_filter_last_known();
    } else if (is_filter && !status->online) {
        update_filter_last_known_age();
    }
}

static bool co2_set_allowed_interval(const fishduino_settings_t *settings)
{
    uint32_t interval_ms = (uint32_t)settings->co2_command_min_interval_s * 1000U;
    if (interval_ms < FISHDUINO_SHELLY_CO2_CMD_MIN_MS) {
        interval_ms = FISHDUINO_SHELLY_CO2_CMD_MIN_MS;
    }
    uint32_t t = now_ms();
    if (s_state.last_co2_command_ms != 0 && (t - s_state.last_co2_command_ms) < interval_ms) {
        return false;
    }
    return true;
}

static void execute_co2_set(const fishduino_settings_t *settings, bool on)
{
    if (!settings->shelly_co2.enabled) {
        return;
    }

    if (!co2_set_allowed_interval(settings)) {
        return;
    }

    if (fishduino_shelly_co2_set_output(settings, on)) {
        s_state.co2_last_sent_on = on;
        s_state.last_co2_command_ms = now_ms();
        s_state.co2_status.output = on;
    }
}

typedef struct {
    float sum_watts;
    unsigned sample_count;
    uint32_t start_ms;
} filter_cal_ctx_t;

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

static void run_filter_calibrate(const fishduino_settings_t *settings)
{
    if (!settings->shelly_filter.enabled) {
        ESP_LOGW(TAG, "Filter calibrate: filter plug disabled");
        return;
    }

    state_lock();
    s_state.filter_calibrating = true;
    s_state.filter_calibrate_progress_s = 0;
    state_unlock();

    filter_cal_ctx_t cal = {0};
    uint32_t start = now_ms();
    const uint32_t duration_ms = 30000;

    while ((now_ms() - start) < duration_ms) {
        fishduino_settings_t snap;
        if (!fishduino_settings_get_snapshot(&snap)) {
            break;
        }

        poll_plug(&snap.shelly_filter, &s_state.filter_status, true);

        state_lock();
        if (s_state.filter_status.online && s_state.filter_status.output &&
            s_state.filter_status.watts >= 0.0f) {
            cal.sum_watts += s_state.filter_status.watts;
            cal.sample_count++;
        }
        uint8_t prog = (uint8_t)((now_ms() - start) / 1000U);
        if (prog > 30) {
            prog = 30;
        }
        s_state.filter_calibrate_progress_s = prog;
        state_unlock();

        vTaskDelay(pdMS_TO_TICKS(FISHDUINO_SHELLY_POLL_MS));
    }

    state_lock();
    s_state.filter_calibrating = false;
    s_state.filter_calibrate_progress_s = 30;
    state_unlock();

    if (cal.sample_count > 0) {
        cal.start_ms = start;
        fishduino_settings_update(mutator_filter_calibrate, &cal, true);
    } else {
        ESP_LOGW(TAG, "Filter calibrate: no samples (filter on and online?)");
    }

    fishduino_settings_t snap;
    if (fishduino_settings_get_snapshot(&snap)) {
        state_lock();
        update_filter_alarm(&snap);
        state_unlock();
    }
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
        bool had_cmd = false;

        while (xQueueReceive(s_cmd_queue, &cmd, 0) == pdTRUE) {
            had_cmd = true;
            if (cmd.type == SHELLY_CMD_CO2_SET) {
                state_lock();
                execute_co2_set(&settings, cmd.on);
                state_unlock();
            } else if (cmd.type == SHELLY_CMD_FILTER_CALIBRATE) {
                run_filter_calibrate(&settings);
                had_cmd = false;
            }
        }

        uint32_t t = now_ms();
        if (had_cmd || (t - last_poll) >= FISHDUINO_SHELLY_POLL_MS) {
            last_poll = t;

            state_lock();
            s_state.last_poll_ms = t;

            if (settings.shelly_co2.enabled) {
                poll_plug(&settings.shelly_co2, &s_state.co2_status, false);
            }
            if (settings.shelly_filter.enabled) {
                bool was_off = !s_state.filter_status.output;
                poll_plug(&settings.shelly_filter, &s_state.filter_status, true);
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

    state_lock();
    s_state.co2_desired_on = target;
    s_state.co2_waiting_time = !now->valid_time;
    if (!now->valid_time) {
        target = false;
        s_state.co2_desired_on = false;
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
    state_lock();
    poll_plug(&settings.shelly_filter, &s_state.filter_status, true);
    update_filter_alarm(&settings);
    state_unlock();
    return true;
}

#include "dashboard_data.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

#include "ble/ble_central_manager.h"
#include "co2/co2_gpio.h"
#include "feeder/feeder_actuator.h"
#include "maintenance/maintenance_mode.h"
#include "net/wifi_manager.h"
#include "scheduler/scheduler.h"

#define TEMP_HISTORY_CAP DASHBOARD_TEMP_HISTORY_LEN
static float s_temp_ring[TEMP_HISTORY_CAP];
static uint8_t s_temp_ring_count;
static uint8_t s_temp_ring_head;

static void temp_history_push(float temp_f)
{
    s_temp_ring[s_temp_ring_head] = temp_f;
    s_temp_ring_head = (uint8_t)((s_temp_ring_head + 1U) % TEMP_HISTORY_CAP);
    if (s_temp_ring_count < TEMP_HISTORY_CAP) {
        s_temp_ring_count++;
    }
}

static void temp_history_copy(dashboard_snapshot_t *out)
{
    out->temp_history_count = s_temp_ring_count;
    if (s_temp_ring_count == 0) {
        out->temp_trend_valid = false;
        return;
    }

    uint8_t start = 0;
    if (s_temp_ring_count == TEMP_HISTORY_CAP) {
        start = s_temp_ring_head;
    }

    float min_v = 999.0f;
    float max_v = -999.0f;
    for (uint8_t i = 0; i < s_temp_ring_count; i++) {
        uint8_t idx = (uint8_t)((start + i) % TEMP_HISTORY_CAP);
        float v = s_temp_ring[idx];
        out->temp_history[i] = v;
        if (v < min_v) {
            min_v = v;
        }
        if (v > max_v) {
            max_v = v;
        }
    }

    out->temp_trend_min_f = min_v;
    out->temp_trend_max_f = max_v;
    out->temp_trend_valid = s_temp_ring_count >= 2;
}

static void format_minutes(uint16_t min, char *buf, size_t len)
{
    snprintf(buf, len, "%02u:%02u", (unsigned)(min / 60), (unsigned)(min % 60));
}

static bool water_field_valid(const water_test_entry_t *e, uint8_t bit)
{
    return e != NULL && (e->valid_flags & bit) != 0;
}

static uint16_t feeder_next_feed_min(const fishduino_feeder_t *feeder, uint16_t now_min)
{
    uint16_t slots[2];
    size_t n = 0;

    if (feeder->settings.feeder.feed_min_1 != 0xFFFF) {
        slots[n++] = feeder->settings.feeder.feed_min_1;
    }
    if (feeder->settings.feeder.feed_min_2 != 0xFFFF) {
        slots[n++] = feeder->settings.feeder.feed_min_2;
    }
    if (n == 0) {
        return 0xFFFF;
    }

    uint16_t next = 0xFFFF;
    for (size_t i = 0; i < n; i++) {
        if (slots[i] > now_min && slots[i] < next) {
            next = slots[i];
        }
    }
    if (next != 0xFFFF) {
        return next;
    }

    next = slots[0];
    for (size_t i = 1; i < n; i++) {
        if (slots[i] < next) {
            next = slots[i];
        }
    }
    return next;
}

static dashboard_led_state_t wifi_led_state(void)
{
    fishduino_wifi_status_t st;
    fishduino_wifi_get_status(&st);
    switch (st.kind) {
    case FISHDUINO_WIFI_STATUS_CONNECTED:
        return DASHBOARD_LED_OK;
    case FISHDUINO_WIFI_STATUS_CONNECTING:
        return DASHBOARD_LED_WARN;
    case FISHDUINO_WIFI_STATUS_UNAVAILABLE:
    case FISHDUINO_WIFI_STATUS_CREDENTIALS_MISSING:
        return DASHBOARD_LED_NA;
    default:
        return DASHBOARD_LED_FAULT;
    }
}

static void fill_heater_ble(dashboard_snapshot_t *out, const fishduino_settings_t *settings)
{
    heater_status_t hs;
    heater_manager_get_status(&hs);

    out->heater_enabled = settings->heater.enabled;
    out->temp_f = hs.reported_temp_f;
    out->setpoint_f = hs.target_temp_f;
    out->temp_valid = hs.online && !hs.stale && hs.reported_temp_f > 32.0f;
    out->temp_stale = hs.stale;
    out->heater_state = hs.state;
    out->heater_heating = hs.heating;
    out->heater_online = hs.online;
    snprintf(out->heater_state_text, sizeof(out->heater_state_text), "%s",
             heater_manager_state_text(hs.state));

    if (!settings->heater.enabled) {
        snprintf(out->heater_relay_text, sizeof(out->heater_relay_text), "CHIHIROS OFF");
    } else if (!hs.online) {
        snprintf(out->heater_relay_text, sizeof(out->heater_relay_text), "CHIHIROS OFFLINE");
    } else if (hs.stale) {
        snprintf(out->heater_relay_text, sizeof(out->heater_relay_text), "CHIHIROS STALE");
    } else if (hs.heating) {
        snprintf(out->heater_relay_text, sizeof(out->heater_relay_text), "CHIHIROS HEATING");
    } else {
        snprintf(out->heater_relay_text, sizeof(out->heater_relay_text), "CHIHIROS IDLE");
    }
}

static void fill_heater_shelly_na(dashboard_snapshot_t *out, const fishduino_settings_t *settings)
{
    out->heater_shelly_enabled = settings->shelly_heater.enabled;
    out->heater_shelly_online = false;
    out->heater_shelly_relay_on = false;
    out->heater_shelly_stale = true;
    out->heater_shelly_last_watts = 0.0f;
    if (!settings->shelly_heater.enabled) {
        snprintf(out->heater_shelly_text, sizeof(out->heater_shelly_text), "SHELLY HEATER DISABLED");
    } else {
        snprintf(out->heater_shelly_text, sizeof(out->heater_shelly_text), "SHELLY HEATER UNKNOWN");
    }
}

static void fill_heater_shelly(dashboard_snapshot_t *out, const fishduino_settings_t *settings,
                               const fishduino_shelly_state_t *ss)
{
    const fishduino_shelly_switch_status_t *hs = &ss->heater_status;

    out->heater_shelly_enabled = settings->shelly_heater.enabled;
    if (!settings->shelly_heater.enabled) {
        fill_heater_shelly_na(out, settings);
        return;
    }

    out->heater_shelly_online = hs->online;
    out->heater_shelly_relay_on = hs->online && hs->output;
    out->heater_shelly_stale = !hs->online;
    out->heater_shelly_last_watts = hs->watts;

    if (!hs->online) {
        snprintf(out->heater_shelly_text, sizeof(out->heater_shelly_text), "SHELLY RELAY OFFLINE");
    } else if (hs->output) {
        snprintf(out->heater_shelly_text, sizeof(out->heater_shelly_text), "SHELLY RELAY ON %.1fW",
                 (double)hs->watts);
    } else {
        snprintf(out->heater_shelly_text, sizeof(out->heater_shelly_text), "SHELLY RELAY OFF");
    }
}

static void fill_filter(dashboard_snapshot_t *out, const fishduino_settings_t *settings,
                        const fishduino_shelly_state_t *ss, bool ss_ok)
{
    out->filter_enabled = settings->shelly_filter.enabled;
    out->filter_baseline_watts = settings->filter_baseline_watts;
    out->filter_threshold_watts = settings->filter_running_watts_threshold;

    if (!ss_ok || !settings->shelly_filter.enabled) {
        out->filter_alarm = FISHDUINO_FILTER_ALARM_NONE;
        out->filter_online = false;
        out->filter_watts = 0.0f;
        snprintf(out->filter_health_text, sizeof(out->filter_health_text),
                 settings->shelly_filter.enabled ? "UNKNOWN" : "DISABLED");
        return;
    }

    out->filter_alarm = ss->filter_alarm;
    out->filter_calibrating = ss->filter_calibrating;
    out->filter_calibrate_progress_s = ss->filter_calibrate_progress_s;

    if (ss->filter_alarm == FISHDUINO_FILTER_ALARM_OFFLINE) {
        out->filter_watts = ss->filter_last_known.watts;
        out->filter_online = false;
        snprintf(out->filter_health_text, sizeof(out->filter_health_text), "OFFLINE");
    } else {
        out->filter_watts = ss->filter_status.watts;
        out->filter_online = ss->filter_status.online;
        switch (ss->filter_alarm) {
        case FISHDUINO_FILTER_ALARM_OFF:
            snprintf(out->filter_health_text, sizeof(out->filter_health_text), "OFF");
            break;
        case FISHDUINO_FILTER_ALARM_LOW_POWER:
            snprintf(out->filter_health_text, sizeof(out->filter_health_text), "LOW POWER");
            break;
        default:
            snprintf(out->filter_health_text, sizeof(out->filter_health_text), "HEALTHY");
            break;
        }
    }
}

static void fill_co2(dashboard_snapshot_t *out, const fishduino_co2_t *co2,
                     const fishduino_settings_t *settings, const fishduino_shelly_state_t *ss,
                     bool ss_ok)
{
    out->co2_enabled = settings->shelly_co2.enabled || fishduino_co2_gpio_is_configured();
    out->co2_schedule_enabled = settings->co2.enabled;
    out->co2_manual_override = settings->co2.manual_override;

    if (ss_ok) {
        out->co2_block_reason = ss->co2_block_reason;
        out->co2_waiting_time = ss->co2_waiting_time;
        out->co2_manual = ss->co2_manual_active;
    } else {
        out->co2_block_reason = CO2_BLOCK_NONE;
        out->co2_waiting_time = false;
        out->co2_manual = settings->co2.manual_override;
    }

    if (settings->shelly_co2.enabled) {
        if (ss_ok) {
            out->co2_online = ss->co2_status.online;
            out->co2_desired_on = ss->co2_desired_on;
            out->co2_relay_known = ss->co2_status.online;
            out->co2_relay_on = ss->co2_status.online && ss->co2_status.output;
            out->co2_on = out->co2_relay_on;
        } else {
            out->co2_online = false;
            out->co2_desired_on = false;
            out->co2_relay_known = false;
            out->co2_relay_on = false;
            out->co2_on = false;
        }

        if (!settings->co2.enabled) {
            snprintf(out->co2_state_text, sizeof(out->co2_state_text), "SCHED OFF");
        } else if (!ss_ok || !out->co2_online) {
            snprintf(out->co2_state_text, sizeof(out->co2_state_text), "OFFLINE");
        } else if (out->co2_waiting_time) {
            snprintf(out->co2_state_text, sizeof(out->co2_state_text), "NO TIME");
        } else if (out->co2_desired_on != out->co2_relay_on) {
            snprintf(out->co2_state_text, sizeof(out->co2_state_text), "REQ %s",
                     out->co2_desired_on ? "ON" : "OFF");
        } else {
            snprintf(out->co2_state_text, sizeof(out->co2_state_text), "%s",
                     out->co2_relay_on ? "ON" : "OFF");
        }
    } else {
        bool gpio_on = fishduino_co2_get_output(co2);
        out->co2_online = fishduino_co2_gpio_is_configured();
        out->co2_desired_on = gpio_on;
        out->co2_relay_known = out->co2_online;
        out->co2_relay_on = gpio_on;
        out->co2_on = gpio_on;
        snprintf(out->co2_state_text, sizeof(out->co2_state_text), "%s", gpio_on ? "ON" : "OFF");
    }

    if (ss_ok && out->co2_block_reason != CO2_BLOCK_NONE && !out->co2_relay_on) {
        snprintf(out->co2_block_text, sizeof(out->co2_block_text), "%s",
                 fishduino_co2_safety_reason_text(out->co2_block_reason));
    } else if (out->co2_relay_known && out->co2_desired_on != out->co2_relay_on) {
        snprintf(out->co2_block_text, sizeof(out->co2_block_text), "RELAY %s",
                 out->co2_relay_on ? "ON" : "OFF");
    } else if (!out->co2_relay_known && settings->shelly_co2.enabled) {
        snprintf(out->co2_block_text, sizeof(out->co2_block_text), "RELAY UNKNOWN");
    } else {
        snprintf(out->co2_block_text, sizeof(out->co2_block_text), "NONE");
    }
}

static void fill_feeder(dashboard_snapshot_t *out, const fishduino_feeder_t *feeder,
                        const fishduino_time_snapshot_t *now)
{
    out->feeder_configured = fishduino_feeder_actuator_is_configured();
    bool slots_enabled =
        feeder->settings.feeder.feed_min_1 != 0xFFFF || feeder->settings.feeder.feed_min_2 != 0xFFFF;
    out->feeder_scheduled = out->feeder_configured && slots_enabled;
    out->next_feed_valid = false;
    out->next_feed_text[0] = '\0';

    if (!out->feeder_configured) {
        snprintf(out->next_feed_text, sizeof(out->next_feed_text), "NO GPIO");
    } else if (!out->feeder_scheduled) {
        snprintf(out->next_feed_text, sizeof(out->next_feed_text), "DISABLED");
    } else if (!now->valid_time) {
        snprintf(out->next_feed_text, sizeof(out->next_feed_text), "NO TIME");
    } else {
        out->next_feed_min = feeder_next_feed_min(feeder, now->minutes_since_midnight);
        out->next_feed_valid = out->next_feed_min != 0xFFFF;
        if (out->next_feed_valid) {
            format_minutes(out->next_feed_min, out->next_feed_text, sizeof(out->next_feed_text));
        } else {
            snprintf(out->next_feed_text, sizeof(out->next_feed_text), "TIME?");
        }
    }
}

static void fill_lighting(dashboard_snapshot_t *out, const fishduino_settings_t *settings)
{
    fishduino_fluval_get_state(&out->fluval);
    out->fluval_link = fishduino_fluval_get_link_status();

    if (out->fluval_link == FISHDUINO_FLUVAL_LINK_DISABLED) {
        snprintf(out->light_mode_label, sizeof(out->light_mode_label), "OFF");
        out->light_intensity_pct = 0;
        snprintf(out->light_sunrise_text, sizeof(out->light_sunrise_text), "--:--");
        snprintf(out->light_sunset_text, sizeof(out->light_sunset_text), "--:--");
    } else if (out->fluval.mode == FISHDUINO_FLUVAL_MODE_AUTO) {
        snprintf(out->light_mode_label, sizeof(out->light_mode_label), "DAYLIGHT");
        out->light_intensity_pct = out->fluval.avg_output;
        snprintf(out->light_sunrise_text, sizeof(out->light_sunrise_text), "AUTO");
        snprintf(out->light_sunset_text, sizeof(out->light_sunset_text), "AUTO");
    } else if (out->fluval.mode == FISHDUINO_FLUVAL_MODE_MANUAL) {
        snprintf(out->light_mode_label, sizeof(out->light_mode_label), "MANUAL");
        out->light_intensity_pct = out->fluval.avg_output;
        snprintf(out->light_sunrise_text, sizeof(out->light_sunrise_text), "--:--");
        snprintf(out->light_sunset_text, sizeof(out->light_sunset_text), "--:--");
    } else {
        snprintf(out->light_mode_label, sizeof(out->light_mode_label), "LINKING");
        out->light_intensity_pct = 0;
        snprintf(out->light_sunrise_text, sizeof(out->light_sunrise_text), "--:--");
        snprintf(out->light_sunset_text, sizeof(out->light_sunset_text), "--:--");
    }

    (void)settings;
}

void dashboard_data_refresh(dashboard_snapshot_t *out, const fishduino_co2_t *co2,
                            const fishduino_feeder_t *feeder, const fishduino_settings_t *settings)
{
    memset(out, 0, sizeof(*out));

    fishduino_time_snapshot_t now;
    fishduino_time_snapshot_now(&now);

    fill_heater_ble(out, settings);

    fishduino_shelly_state_t ss;
    bool ss_ok = fishduino_shelly_manager_get_state_snapshot(&ss);

    if (ss_ok) {
        fill_heater_shelly(out, settings, &ss);
        fill_filter(out, settings, &ss, true);
        fill_co2(out, co2, settings, &ss, true);
    } else {
        fill_heater_shelly_na(out, settings);
        fill_filter(out, settings, NULL, false);
        fill_co2(out, co2, settings, NULL, false);
    }

    fill_feeder(out, feeder, &now);

    if (out->temp_valid) {
        temp_history_push(out->temp_f);
    }
    temp_history_copy(out);

    if (water_metrics_get_latest(&out->water_latest) == ESP_OK) {
        out->water_has_entry = true;
        char alert_msg[64];
        out->water_alert = water_alerts_classify(&out->water_latest, alert_msg, sizeof(alert_msg));
        snprintf(out->water_alert_text, sizeof(out->water_alert_text), "%s",
                 water_alerts_level_text(out->water_alert));

        const water_test_entry_t *we = &out->water_latest;
        out->water_ph_ok = water_field_valid(we, WATER_VALID_PH) && we->ph >= WATER_ALERT_PH_MIN_DEFAULT &&
                           we->ph <= WATER_ALERT_PH_MAX_DEFAULT;
        out->water_nh3_ok = water_field_valid(we, WATER_VALID_AMMONIA) && we->ammonia_ppm <= 0.0f;
        out->water_no2_ok = water_field_valid(we, WATER_VALID_NITRITE) && we->nitrite_ppm <= 0.0f;
        out->water_no3_ok = water_field_valid(we, WATER_VALID_NITRATE) &&
                            we->nitrate_ppm <= WATER_ALERT_NITRATE_MAX_PPM;

        if (now.valid_time && we->timestamp_unix > 0 && !(we->valid_flags & WATER_FLAG_TIME_UNKNOWN)) {
            time_t t = (time_t)we->timestamp_unix;
            struct tm tm_local;
            if (localtime_r(&t, &tm_local) != NULL) {
                strftime(out->water_updated_text, sizeof(out->water_updated_text), "%I:%M %p", &tm_local);
            }
        } else {
            snprintf(out->water_updated_text, sizeof(out->water_updated_text), "TIME UNKNOWN");
        }
    } else {
        snprintf(out->water_alert_text, sizeof(out->water_alert_text), "NO TEST");
        snprintf(out->water_updated_text, sizeof(out->water_updated_text), "--");
    }

    maintenance_task_t due[MAINT_TASK_COUNT];
    size_t due_count = 0;
    maintenance_tracker_get_due_tasks(due, MAINT_TASK_COUNT, &due_count);
    out->reminders_due_count = due_count;
    out->reminders_any = due_count > 0;
    if (due_count > 0) {
        const maintenance_task_t *show = &due[0];
        for (size_t i = 0; i < due_count; i++) {
            if (maintenance_tracker_task_status(&due[i]) == MAINT_TRACKER_STATUS_OVERDUE) {
                show = &due[i];
                break;
            }
        }
        strncpy(out->next_reminder_name, show->name, sizeof(out->next_reminder_name) - 1);
        out->next_reminder_status = maintenance_tracker_task_status(show);
    }

    out->time_valid = now.valid_time;
    if (now.valid_time) {
        time_t t = (time_t)now.epoch_seconds;
        struct tm tm_local;
        if (localtime_r(&t, &tm_local) != NULL) {
            strftime(out->clock_time, sizeof(out->clock_time), "%I:%M %p", &tm_local);
            strftime(out->clock_date, sizeof(out->clock_date), "%b %d, %Y", &tm_local);
        }
    } else {
        snprintf(out->clock_time, sizeof(out->clock_time), "--:--");
        snprintf(out->clock_date, sizeof(out->clock_date), "NO TIME");
    }

    out->maintenance_mode_active = fishduino_maintenance_mode_is_active();
    if (out->maintenance_mode_active) {
        out->maintenance_mode_remaining_min = fishduino_maintenance_mode_remaining_ms() / 60000LL;
        snprintf(out->mode_text, sizeof(out->mode_text), "MAINT");
    } else if (out->co2_manual_override) {
        snprintf(out->mode_text, sizeof(out->mode_text), "MANUAL");
    } else if (!out->co2_schedule_enabled) {
        snprintf(out->mode_text, sizeof(out->mode_text), "SCHED OFF");
    } else {
        snprintf(out->mode_text, sizeof(out->mode_text), "AUTO");
    }

    out->led_wifi = wifi_led_state();
    if (!ss_ok) {
        out->led_shelly_co2 = settings->shelly_co2.enabled ? DASHBOARD_LED_FAULT : DASHBOARD_LED_NA;
        out->led_shelly_filter = settings->shelly_filter.enabled ? DASHBOARD_LED_FAULT : DASHBOARD_LED_NA;
        out->led_shelly_heater = settings->shelly_heater.enabled ? DASHBOARD_LED_FAULT : DASHBOARD_LED_NA;
    } else {
        out->led_shelly_co2 = settings->shelly_co2.enabled
                                  ? (ss.co2_status.online ? DASHBOARD_LED_OK : DASHBOARD_LED_FAULT)
                                  : DASHBOARD_LED_NA;
        out->led_shelly_filter = settings->shelly_filter.enabled
                                     ? (ss.filter_alarm == FISHDUINO_FILTER_ALARM_OFFLINE ? DASHBOARD_LED_FAULT
                                                                                          : DASHBOARD_LED_OK)
                                     : DASHBOARD_LED_NA;
        out->led_shelly_heater = settings->shelly_heater.enabled
                                     ? (ss.heater_status.online ? DASHBOARD_LED_OK : DASHBOARD_LED_FAULT)
                                     : DASHBOARD_LED_NA;
    }

    out->led_ble = ble_central_manager_is_ready() ? DASHBOARD_LED_OK : DASHBOARD_LED_WARN;
    out->led_feeder = out->feeder_configured
                          ? (out->feeder_scheduled ? DASHBOARD_LED_OK : DASHBOARD_LED_WARN)
                          : DASHBOARD_LED_NA;

    fill_lighting(out, settings);
    out->led_light = settings->fluval.enabled
                         ? (out->fluval_link == FISHDUINO_FLUVAL_LINK_OK ? DASHBOARD_LED_OK
                                                                          : DASHBOARD_LED_WARN)
                         : DASHBOARD_LED_NA;

    heater_status_t hs;
    heater_manager_get_status(&hs);

    out->led_alerts = DASHBOARD_LED_OK;
    if (ss_ok && ss.filter_alarm != FISHDUINO_FILTER_ALARM_NONE) {
        out->led_alerts = DASHBOARD_LED_WARN;
    }
    if (hs.alarm != HEATER_ALARM_NONE || out->water_alert >= WATER_ALERT_WARNING || out->reminders_any) {
        out->led_alerts = DASHBOARD_LED_WARN;
    }

    out->banner_visible = false;
    if (out->maintenance_mode_active) {
        out->banner_visible = true;
        snprintf(out->banner_text, sizeof(out->banner_text), "MAINTENANCE MODE (%lld MIN)",
                 (long long)out->maintenance_mode_remaining_min);
    } else if (ss_ok && ss.filter_alarm == FISHDUINO_FILTER_ALARM_OFF && ss.alert_blink_on) {
        out->banner_visible = true;
        out->banner_critical = true;
        snprintf(out->banner_text, sizeof(out->banner_text), "!!! FILTER IS OFF !!!");
    } else if (ss_ok && ss.filter_alarm == FISHDUINO_FILTER_ALARM_OFF) {
        out->banner_visible = true;
        out->banner_critical = true;
        snprintf(out->banner_text, sizeof(out->banner_text), "FILTER IS OFF");
    } else if (ss_ok && ss.filter_alarm == FISHDUINO_FILTER_ALARM_OFFLINE) {
        out->banner_visible = true;
        snprintf(out->banner_text, sizeof(out->banner_text), "FILTER MONITOR OFFLINE");
    } else if (ss_ok && ss.filter_alarm == FISHDUINO_FILTER_ALARM_LOW_POWER) {
        out->banner_visible = true;
        snprintf(out->banner_text, sizeof(out->banner_text), "FILTER POWER LOW");
    } else if (ss_ok && ss.filter_calibrating) {
        out->banner_visible = true;
        snprintf(out->banner_text, sizeof(out->banner_text), "FILTER CALIBRATING %us/30s",
                 (unsigned)ss.filter_calibrate_progress_s);
    }
}

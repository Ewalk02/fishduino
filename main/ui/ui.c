#include "ui.h"

#include <stdio.h>
#include <string.h>

#include "co2/co2_gpio.h"
#include "feeder/feeder_actuator.h"
#include "net/wifi_manager.h"
#include "screen_commissioning.h"
#include "screen_diagnostics.h"
#include "screen_dashboard.h"
#include "screen_options.h"
#include "shelly/shelly_manager.h"

static void format_minutes(uint16_t min, char *buf, size_t len)
{
    snprintf(buf, len, "%02u:%02u", (unsigned)(min / 60), (unsigned)(min % 60));
}

static void format_age_s(uint32_t age_ms, char *buf, size_t len)
{
    uint32_t sec = age_ms / 1000U;
    if (sec < 60) {
        snprintf(buf, len, "%lus ago", (unsigned long)sec);
    } else {
        snprintf(buf, len, "%lum ago", (unsigned long)(sec / 60U));
    }
}

void fishduino_ui_init(fishduino_ui_t *ui)
{
    ui->root = lv_obj_create(NULL);
    lv_obj_remove_style_all(ui->root);
    lv_obj_set_size(ui->root, LV_PCT(100), LV_PCT(100));

    fishduino_dashboard_handles_t h = fishduino_screen_dashboard_build(ui->root);
    ui->label_co2 = h.label_co2;
    ui->label_co2_detail = h.label_co2_detail;
    ui->label_feeder = h.label_feeder;
    ui->label_filter = h.label_filter;
    ui->label_filter_energy = h.label_filter_energy;
    ui->label_filter_alarm = h.label_filter_alarm;
    ui->label_wifi = h.label_wifi;

    fishduino_screen_options_build(ui->root);

    lv_screen_load(ui->root);
}

void fishduino_ui_update(fishduino_ui_t *ui, const fishduino_co2_t *co2,
                         const fishduino_feeder_t *feeder, const fishduino_settings_t *settings)
{
    fishduino_shelly_state_t ss;
    if (!fishduino_shelly_manager_get_state_snapshot(&ss)) {
        return;
    }

    char buf[160];
    char age_s[24];

    (void)co2;
    (void)feeder;

    fishduino_screen_commissioning_tick();
    fishduino_screen_diagnostics_refresh();

    if (ui->label_wifi) {
        lv_label_set_text(ui->label_wifi, fishduino_wifi_status_text());
    }

    if (ui->label_filter_alarm) {
        if (ss.filter_alarm == FISHDUINO_FILTER_ALARM_OFF && ss.alert_blink_on) {
            lv_label_set_text(ui->label_filter_alarm, "!!! FILTER IS OFF !!!");
            lv_obj_clear_flag(ui->label_filter_alarm, LV_OBJ_FLAG_HIDDEN);
        } else if (ss.filter_alarm == FISHDUINO_FILTER_ALARM_OFF) {
            lv_label_set_text(ui->label_filter_alarm, "FILTER IS OFF");
            lv_obj_clear_flag(ui->label_filter_alarm, LV_OBJ_FLAG_HIDDEN);
        } else if (ss.filter_alarm == FISHDUINO_FILTER_ALARM_OFFLINE) {
            lv_label_set_text(ui->label_filter_alarm, "FILTER MONITOR OFFLINE");
            lv_obj_clear_flag(ui->label_filter_alarm, LV_OBJ_FLAG_HIDDEN);
        } else if (ss.filter_alarm == FISHDUINO_FILTER_ALARM_LOW_POWER) {
            lv_label_set_text(ui->label_filter_alarm, "FILTER POWER LOW");
            lv_obj_clear_flag(ui->label_filter_alarm, LV_OBJ_FLAG_HIDDEN);
        } else if (ss.filter_calibrating) {
            snprintf(buf, sizeof(buf), "Filter calibrating... %us/30s", (unsigned)ss.filter_calibrate_progress_s);
            lv_label_set_text(ui->label_filter_alarm, buf);
            lv_obj_clear_flag(ui->label_filter_alarm, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_label_set_text(ui->label_filter_alarm, "");
            lv_obj_add_flag(ui->label_filter_alarm, LV_OBJ_FLAG_HIDDEN);
        }
    }

    if (ui->label_co2) {
        if (settings->shelly_co2.enabled) {
            if (!settings->co2.enabled) {
                snprintf(buf, sizeof(buf), "CO2: SCHEDULE DISABLED");
            } else if (ss.co2_waiting_time) {
                snprintf(buf, sizeof(buf), "CO2: waiting for time sync");
            } else if (!ss.co2_status.online) {
                snprintf(buf, sizeof(buf), "CO2: OFFLINE");
            } else if (ss.co2_manual_active) {
                snprintf(buf, sizeof(buf), "CO2: %s (MANUAL)", ss.co2_status.output ? "ON" : "OFF");
            } else {
                snprintf(buf, sizeof(buf), "CO2: %s", ss.co2_desired_on ? "ON" : "OFF");
            }
        } else if (!fishduino_co2_gpio_is_configured()) {
            snprintf(buf, sizeof(buf), "CO2: GPIO not configured");
        } else {
            snprintf(buf, sizeof(buf), "CO2: %s", fishduino_co2_get_output(co2) ? "ON" : "OFF");
        }
        lv_label_set_text(ui->label_co2, buf);
    }

    if (ui->label_co2_detail && settings->shelly_co2.enabled) {
        char on_s[8], off_s[8];
        format_minutes(settings->co2.on_min, on_s, sizeof(on_s));
        format_minutes(settings->co2.off_min, off_s, sizeof(off_s));
        const char *midnight = (settings->co2.on_min > settings->co2.off_min) ? " crosses midnight" : "";
        if (!settings->co2.enabled) {
            snprintf(buf, sizeof(buf), "Sched: %s-%s (off)%s  %.1f W", on_s, off_s, midnight,
                     (double)ss.co2_status.watts);
        } else {
            snprintf(buf, sizeof(buf), "CO2 W: %.1f  Sched: %s-%s%s", (double)ss.co2_status.watts, on_s, off_s,
                     midnight);
        }
        lv_label_set_text(ui->label_co2_detail, buf);
    } else if (ui->label_co2_detail) {
        lv_label_set_text(ui->label_co2_detail, "");
    }

    if (ui->label_filter && settings->shelly_filter.enabled) {
        if (ss.filter_alarm == FISHDUINO_FILTER_ALARM_OFFLINE) {
            format_age_s(ss.filter_last_known_age_ms, age_s, sizeof(age_s));
            snprintf(buf, sizeof(buf), "Filter: OFFLINE (last: %.1f W, %s)",
                     (double)ss.filter_last_known.watts, age_s);
        } else {
            const char *state = "RUNNING";
            if (ss.filter_alarm == FISHDUINO_FILTER_ALARM_OFF) {
                state = "OFF";
            } else if (ss.filter_alarm == FISHDUINO_FILTER_ALARM_LOW_POWER) {
                state = "LOW POWER";
            }
            snprintf(buf, sizeof(buf), "Filter: %s  %.1f W", state, (double)ss.filter_status.watts);
        }
        if (settings->filter_baseline_watts > 0.0f) {
            char extra[64];
            snprintf(extra, sizeof(extra), " | base %.0fW thr %.0fW", (double)settings->filter_baseline_watts,
                     (double)settings->filter_running_watts_threshold);
            strncat(buf, extra, sizeof(buf) - strlen(buf) - 1);
        }
        lv_label_set_text(ui->label_filter, buf);
    } else if (ui->label_filter) {
        lv_label_set_text(ui->label_filter, "Filter: (Shelly disabled)");
    }

    if (ui->label_filter_energy && settings->shelly_filter.enabled &&
        ss.filter_alarm != FISHDUINO_FILTER_ALARM_OFF) {
        if (ss.filter_alarm == FISHDUINO_FILTER_ALARM_OFFLINE &&
            ss.filter_last_known.last_success_ms != 0) {
            format_age_s(ss.filter_last_known_age_ms, age_s, sizeof(age_s));
            snprintf(buf, sizeof(buf), "Last: out=%s %.1fW %s  Alarm: OFFLINE",
                     ss.filter_last_known.output ? "on" : "off", (double)ss.filter_last_known.watts, age_s);
        } else {
            snprintf(buf, sizeof(buf), "Filter kWh: %.2f  Alarm: %s",
                     (double)(ss.filter_status.energy_wh / 1000.0f),
                     fishduino_filter_alarm_text(ss.filter_alarm));
        }
        lv_label_set_text(ui->label_filter_energy, buf);
    } else if (ui->label_filter_energy) {
        lv_label_set_text(ui->label_filter_energy, "");
    }

    if (ui->label_feeder) {
        if (!fishduino_feeder_actuator_is_configured()) {
            lv_label_set_text(ui->label_feeder, "Feeder: GPIO not configured");
        } else {
            lv_label_set_text(ui->label_feeder, "Feeder: ready");
        }
    }
}

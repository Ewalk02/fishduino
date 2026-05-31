#include "dashboard_console.h"

#include <stdio.h>

#include "dashboard_data.h"
#include "esp_console.h"
#include "esp_log.h"
#include "fishduino_app.h"
#include "shelly/shelly_manager.h"

static const char *TAG = "dash_console";

static void print_snapshot(const dashboard_snapshot_t *s)
{
    printf("--- Dashboard snapshot ---\n");
    printf("Temp: %.1fF setpoint=%.1f valid=%s stale=%s\n", (double)s->temp_f, (double)s->setpoint_f,
           s->temp_valid ? "yes" : "no", s->temp_stale ? "yes" : "no");
    printf("Chihiros: %s | %s\n", s->heater_relay_text, s->heater_state_text);
    printf("Shelly heater: en=%s online=%s relay=%s %.1fW | %s\n",
           s->heater_shelly_enabled ? "yes" : "no", s->heater_shelly_online ? "yes" : "no",
           s->heater_shelly_relay_on ? "on" : "off", (double)s->heater_shelly_last_watts,
           s->heater_shelly_text);
    printf("Filter: %.1fW %s alarm=%d\n", (double)s->filter_watts, s->filter_health_text,
           (int)s->filter_alarm);
    printf("CO2: %s desired=%s relay=%s%s online=%s | block: %s\n", s->co2_state_text,
           s->co2_desired_on ? "on" : "off", s->co2_relay_known ? (s->co2_relay_on ? "on" : "off") : "unknown",
           s->co2_relay_known ? "" : "", s->co2_online ? "yes" : "no", s->co2_block_text);
    printf("Feeder: cfg=%s sched=%s next=%s\n", s->feeder_configured ? "yes" : "no",
           s->feeder_scheduled ? "yes" : "no", s->next_feed_text);
    printf("Water: %s %s\n", s->water_has_entry ? s->water_alert_text : "no test",
           s->water_updated_text);
    printf("Reminders: %zu due next=%s\n", s->reminders_due_count,
           s->reminders_any ? s->next_reminder_name : "(none)");
    printf("Clock: %s %s mode=%s maint=%s\n", s->clock_time, s->clock_date, s->mode_text,
           s->maintenance_mode_active ? "ON" : "off");
    printf("Light: %s %u%% rise=%s set=%s\n", s->light_mode_label, (unsigned)s->light_intensity_pct,
           s->light_sunrise_text, s->light_sunset_text);
    if (s->banner_visible) {
        printf("Banner: %s\n", s->banner_text);
    }
    printf("Temp trend: %u samples valid=%s\n", (unsigned)s->temp_history_count,
           s->temp_trend_valid ? "yes" : "no");
}

static int cmd_dashboard_status(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    fishduino_co2_t co2;
    fishduino_feeder_t feeder;
    fishduino_settings_t settings;
    fishduino_app_dashboard_inputs(&co2, &feeder, &settings);

    dashboard_snapshot_t snap;
    dashboard_data_refresh(&snap, &co2, &feeder, &settings);
    print_snapshot(&snap);

    fishduino_shelly_state_t ss;
    if (fishduino_shelly_manager_get_state_snapshot(&ss)) {
        printf("Shelly mgr: co2_desired=%s heater_out=%s filter_alarm=%d\n",
               ss.co2_desired_on ? "on" : "off", ss.heater_status.output ? "on" : "off",
               (int)ss.filter_alarm);
    } else {
        printf("Shelly mgr: snapshot unavailable\n");
    }
    return 0;
}

void fishduino_dashboard_console_register(void)
{
    const esp_console_cmd_t cmd = {
        .command = "dashboard_status",
        .help = "Print cockpit dashboard_data snapshot (headless validation)",
        .func = &cmd_dashboard_status,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
    ESP_LOGI(TAG, "Console: dashboard_status");
}

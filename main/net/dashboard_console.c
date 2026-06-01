#include "dashboard_console.h"

#include <stdio.h>
#include <stdlib.h>

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
    const char *feeder_sched =
        !s->feeder_configured ? "n/a" : (s->feeder_scheduled ? "yes" : "no");
    printf("Feeder: cfg=%s sched=%s next=%s\n", s->feeder_configured ? "yes" : "no", feeder_sched,
           s->next_feed_text);
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

typedef struct {
    fishduino_co2_t co2;
    fishduino_feeder_t feeder;
    fishduino_settings_t settings;
    dashboard_snapshot_t snap;
    fishduino_shelly_state_t shelly;
} dashboard_console_work_t;

static int cmd_dashboard_status(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    dashboard_console_work_t *work = calloc(1, sizeof(*work));
    if (work == NULL) {
        printf("dashboard_status: out of memory\n");
        return 1;
    }

    fishduino_app_dashboard_inputs(&work->co2, &work->feeder, &work->settings);

    dashboard_data_refresh(&work->snap, &work->co2, &work->feeder, &work->settings);
    print_snapshot(&work->snap);

    if (fishduino_shelly_manager_get_state_snapshot(&work->shelly)) {
        printf("Shelly mgr: co2_desired=%s heater_out=%s filter_alarm=%d\n",
               work->shelly.co2_desired_on ? "on" : "off",
               work->shelly.heater_status.output ? "on" : "off",
               (int)work->shelly.filter_alarm);
    } else {
        printf("Shelly mgr: snapshot unavailable\n");
    }

    free(work);
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

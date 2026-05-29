#include "shelly_console.h"

#include <stdio.h>
#include <string.h>

#include "esp_console.h"
#include "esp_log.h"
#include "shelly/shelly_manager.h"
#include "storage/settings_nvs.h"

static const char *TAG = "shelly_console";

static void print_status(const fishduino_shelly_switch_status_t *st, const char *name)
{
    printf("%s: online=%s output=%s %.1fW %.1fV %.2fA %.3fkWh fail=%u\n",
           name, st->online ? "yes" : "no", st->output ? "on" : "off", (double)st->watts,
           (double)st->voltage, (double)st->current, (double)(st->energy_wh / 1000.0f),
           st->fail_count);
    if (st->error_text[0] != '\0') {
        printf("  error: %s\n", st->error_text);
    }
}

static int cmd_shelly_status(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    const fishduino_shelly_state_t *st = fishduino_shelly_manager_get_state();
    print_status(&st->co2_status, "CO2");
    print_status(&st->filter_status, "Filter");
    printf("Filter last-known: output=%s %.1fW age=%lums\n",
           st->filter_last_known.output ? "on" : "off", (double)st->filter_last_known.watts,
           (unsigned long)st->filter_last_known_age_ms);
    printf("co2_desired=%s manual=%s waiting_time=%s sched_enabled=%s\n",
           st->co2_desired_on ? "on" : "off", st->co2_manual_active ? "yes" : "no",
           st->co2_waiting_time ? "yes" : "no",
           fishduino_shelly_manager_get_settings_mutable()->co2.enabled ? "yes" : "no");
    return 0;
}

static int cmd_shelly_co2_on(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    fishduino_shelly_co2_command_now(true);
    printf("CO2 ON queued (Switch.Set CO2 plug only)\n");
    return 0;
}

static int cmd_shelly_co2_off(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    fishduino_shelly_co2_command_now(false);
    printf("CO2 OFF queued\n");
    return 0;
}

static int cmd_shelly_filter(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    fishduino_shelly_poll_filter_now();
    const fishduino_shelly_state_t *st = fishduino_shelly_manager_get_state();
    print_status(&st->filter_status, "Filter");
    printf("read-only: Fishduino never sends Switch.Set to the filter plug\n");
    return 0;
}

static int cmd_shelly_alarm(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    const fishduino_shelly_state_t *st = fishduino_shelly_manager_get_state();
    printf("filter_alarm=%s (%d)\n", fishduino_filter_alarm_text(st->filter_alarm), (int)st->filter_alarm);
    printf("filter_output_off_alert=%s\n", st->filter_output_off_alert ? "yes" : "no");
    printf("last-known: %.1fW output=%s age=%lums\n", (double)st->filter_last_known.watts,
           st->filter_last_known.output ? "on" : "off", (unsigned long)st->filter_last_known_age_ms);
    return 0;
}

static void print_co2_schedule(void)
{
    fishduino_settings_t *st = fishduino_shelly_manager_get_settings_mutable();
    printf("CO2 schedule: %s\n", st->co2.enabled ? "enabled" : "disabled");
    printf("  window: %02u:%02u - %02u:%02u\n", (unsigned)(st->co2.on_min / 60),
           (unsigned)(st->co2.on_min % 60), (unsigned)(st->co2.off_min / 60),
           (unsigned)(st->co2.off_min % 60));
    printf("  timezone: %s\n", fishduino_timezone_name(st->timezone));
}

static int cmd_co2_schedule(int argc, char **argv)
{
    if (argc < 2) {
        print_co2_schedule();
        return 0;
    }

    if (strcmp(argv[1], "on") == 0) {
        fishduino_shelly_set_co2_schedule_enabled(true);
        printf("CO2 schedule enabled (saved NVS)\n");
        return 0;
    }
    if (strcmp(argv[1], "off") == 0) {
        fishduino_shelly_set_co2_schedule_enabled(false);
        printf("CO2 schedule disabled (saved NVS)\n");
        return 0;
    }
    if (strcmp(argv[1], "status") == 0) {
        print_co2_schedule();
        return 0;
    }

    printf("Usage: co2_schedule [on|off|status]\n");
    return 1;
}

void fishduino_shelly_console_register(void)
{
    const esp_console_cmd_t status_cmd = {
        .command = "shelly",
        .help = "Print CO2 and filter Shelly status",
        .func = &cmd_shelly_status,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&status_cmd));

    const esp_console_cmd_t co2_on = {
        .command = "shelly_co2_on",
        .help = "Test CO2 plug ON (Switch.Set)",
        .func = &cmd_shelly_co2_on,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&co2_on));

    const esp_console_cmd_t co2_off = {
        .command = "shelly_co2_off",
        .help = "Test CO2 plug OFF",
        .func = &cmd_shelly_co2_off,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&co2_off));

    const esp_console_cmd_t filter_cmd = {
        .command = "shelly_filter",
        .help = "Poll filter plug (read-only)",
        .func = &cmd_shelly_filter,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&filter_cmd));

    const esp_console_cmd_t alarm_cmd = {
        .command = "shelly_alarm",
        .help = "Show filter alert state",
        .func = &cmd_shelly_alarm,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&alarm_cmd));

    const esp_console_cmd_t co2_sched = {
        .command = "co2_schedule",
        .help = "CO2 schedule: on | off | status",
        .hint = "<on|off|status>",
        .func = &cmd_co2_schedule,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&co2_sched));

    ESP_LOGI(TAG, "Console: shelly, shelly_co2_*, shelly_filter, shelly_alarm, co2_schedule");
}

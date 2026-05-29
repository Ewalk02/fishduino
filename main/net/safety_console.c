#include "safety_console.h"

#include <stdio.h>

#include "esp_console.h"
#include "esp_log.h"
#include "shelly/shelly_manager.h"
#include "storage/settings_runtime.h"

static const char *TAG = "safety";

static int cmd_safety_test(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    fishduino_shelly_state_t ss;
    fishduino_settings_t st;
    fishduino_shelly_manager_get_state_snapshot(&ss);
    fishduino_settings_get_snapshot(&st);

    printf("\n=== Fishduino safety test (use LAMPS, not aquarium gear) ===\n\n");

    printf("1) CO2 test lamp ON\n");
    printf("   Action: shelly_co2_on or dashboard CO2 ON\n");
    printf("   Now: CO2 output=%s %.1fW online=%s\n\n", ss.co2_status.output ? "on" : "off",
           (double)ss.co2_status.watts, ss.co2_status.online ? "yes" : "no");

    printf("2) CO2 test lamp OFF\n");
    printf("   Action: shelly_co2_off or dashboard CO2 OFF\n\n");

    printf("3) CO2 AUTO schedule\n");
    printf("   Action: co2_schedule on, dashboard AUTO\n");
    printf("   Sched: %s %02u:%02u-%02u:%02u desired=%s\n\n", st.co2.enabled ? "enabled" : "disabled",
           (unsigned)(st.co2.on_min / 60), (unsigned)(st.co2.on_min % 60), (unsigned)(st.co2.off_min / 60),
           (unsigned)(st.co2.off_min % 60), ss.co2_desired_on ? "on" : "off");

    printf("4) Filter lamp watts visible\n");
    printf("   Now: Filter %.1fW output=%s online=%s\n\n", (double)ss.filter_status.watts,
           ss.filter_status.output ? "on" : "off", ss.filter_status.online ? "yes" : "no");

    printf("5) Filter output=false -> FILTER IS OFF\n");
    printf("   Now: alarm=%s\n\n", fishduino_filter_alarm_text(ss.filter_alarm));

    printf("6) Filter offline -> FILTER MONITOR OFFLINE\n");
    printf("   Action: unplug filter Shelly or disconnect Wi-Fi\n\n");

    printf("7) Fishduino never Switch.Set on filter plug\n");
    printf("   CO2 IP: %s (enabled=%s id=%d)\n", st.shelly_co2.ip, st.shelly_co2.enabled ? "yes" : "no",
           (int)st.shelly_co2.switch_id);
    printf("   Filter IP: %s (enabled=%s id=%d) READ-ONLY\n\n", st.shelly_filter.ip,
           st.shelly_filter.enabled ? "yes" : "no", (int)st.shelly_filter.switch_id);

    printf("Also: filter_calibrate (30s, filter lamp ON)\n");
    return 0;
}

static int cmd_filter_calibrate(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    fishduino_shelly_filter_calibrate_start();
    printf("Filter calibration started (30s, keep filter lamp ON)\n");
    return 0;
}

void fishduino_safety_console_register(void)
{
    const esp_console_cmd_t safety = {
        .command = "safety_test",
        .help = "Lamp commissioning checklist",
        .func = &cmd_safety_test,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&safety));

    const esp_console_cmd_t cal = {
        .command = "filter_calibrate",
        .help = "30s filter watt baseline (lamp ON)",
        .func = &cmd_filter_calibrate,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&cal));

    ESP_LOGI(TAG, "Console: safety_test, filter_calibrate");
}

#include "status_console.h"

#include <stdio.h>
#include <string.h>

#include "esp_console.h"
#include "esp_log.h"
#include "net/wifi_manager.h"
#include "storage/settings_nvs.h"
#include "storage/settings_runtime.h"
#include "ota/ota_manager.h"
#include "safety/co2_safety.h"
#include "storage/wifi_creds_nvs.h"

static const char *TAG = "status";

void fishduino_status_format(char *buf, size_t len)
{
    if (buf == NULL || len == 0) {
        return;
    }

    fishduino_settings_t st;
    if (!fishduino_settings_get_snapshot(&st)) {
        snprintf(buf, len, "Settings unavailable");
        return;
    }

    char ssid[FISHDUINO_WIFI_SSID_MAX + 1];
    char ip[20];
    fishduino_wifi_get_credentials(ssid, sizeof(ssid), NULL, 0);
    fishduino_wifi_get_sta_ip(ip, sizeof(ip));

    snprintf(buf, len,
             "Wi-Fi SSID: %s\n"
             "Device IP: %s\n"
             "%s\n"
             "CO2 Shelly: %s en=%s id=%d\n"
             "Filter Shelly: %s en=%s id=%d (read-only)\n"
             "Heater Shelly: %s en=%s id=%d\n"
             "CO2 schedule: %s %02u:%02u-%02u:%02u\n"
             "Filter baseline: %.1f W  threshold: %.1f W\n"
             "Firmware: %s\n"
             "CO2 block: %s",
             ssid[0] ? ssid : "(empty)", ip, fishduino_wifi_status_text(), st.shelly_co2.ip,
             st.shelly_co2.enabled ? "yes" : "no", (int)st.shelly_co2.switch_id, st.shelly_filter.ip,
             st.shelly_filter.enabled ? "yes" : "no", (int)st.shelly_filter.switch_id,
             st.shelly_heater.ip, st.shelly_heater.enabled ? "yes" : "no",
             (int)st.shelly_heater.switch_id, st.co2.enabled ? "on" : "off", (unsigned)(st.co2.on_min / 60), (unsigned)(st.co2.on_min % 60),
             (unsigned)(st.co2.off_min / 60), (unsigned)(st.co2.off_min % 60),
             (double)st.filter_baseline_watts, (double)st.filter_running_watts_threshold,
             fishduino_ota_get_version_string(), fishduino_co2_safety_reason_text(fishduino_co2_safety_get_last_block()));
}

static int cmd_status(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    char buf[512];
    fishduino_status_format(buf, sizeof(buf));
    printf("%s\n", buf);
    return 0;
}

void fishduino_status_console_register(void)
{
    const esp_console_cmd_t status = {
        .command = "status",
        .help = "Show active Wi-Fi, Shelly, CO2 schedule, filter calibration",
        .func = &cmd_status,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&status));
    ESP_LOGI(TAG, "Console: status");
}

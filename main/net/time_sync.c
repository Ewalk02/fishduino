#include "time_sync.h"

#include <stdlib.h>
#include <time.h>

#include "esp_log.h"
#include "esp_sntp.h"

static const char *TAG = "time_sync";

static bool s_started = false;
static bool s_done = false;

static const char *posix_tz_for(fishduino_timezone_t tz)
{
    switch (tz) {
    case FISHDUINO_TZ_US_EASTERN:
        return "EST5EDT,M3.2.0,M11.1.0";
    case FISHDUINO_TZ_US_CENTRAL:
        return "CST6CDT,M3.2.0,M11.1.0";
    case FISHDUINO_TZ_US_MOUNTAIN:
        return "MST7MDT,M3.2.0,M11.1.0";
    case FISHDUINO_TZ_US_PACIFIC:
        return "PST8PDT,M3.2.0,M11.1.0";
    default:
        return "CST6CDT,M3.2.0,M11.1.0";
    }
}

void fishduino_time_sync_apply_timezone(const fishduino_settings_t *settings)
{
    if (settings == NULL) {
        return;
    }

    const char *posix = posix_tz_for(settings->timezone);
    setenv("TZ", posix, 1);
    tzset();
    ESP_LOGI(TAG, "Timezone %s (%s)", fishduino_timezone_name(settings->timezone), posix);
}

static void sntp_sync_cb(struct timeval *tv)
{
    (void)tv;
    s_done = true;
    ESP_LOGI(TAG, "SNTP time synced");
}

void fishduino_time_sync_init(void)
{
    s_started = false;
    s_done = false;
}

void fishduino_time_sync_on_wifi_connected(void)
{
    if (s_started) {
        return;
    }

    s_started = true;
    ESP_LOGI(TAG, "Starting SNTP");

    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_set_time_sync_notification_cb(sntp_sync_cb);
    esp_sntp_init();
}

bool fishduino_time_sync_is_done(void)
{
    if (s_done) {
        return true;
    }
    time_t t = time(NULL);
    return t > 100000;
}

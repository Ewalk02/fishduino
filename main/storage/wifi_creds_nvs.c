#include "wifi_creds_nvs.h"

#include <string.h>

#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"

#ifndef CONFIG_ESP_WIFI_SSID
#define CONFIG_ESP_WIFI_SSID ""
#endif
#ifndef CONFIG_ESP_WIFI_PASSWORD
#define CONFIG_ESP_WIFI_PASSWORD ""
#endif

static const char *TAG = "wifi_creds";
static const char *NVS_NAMESPACE = "fishduino";
static const char *NVS_KEY_WIFI_CREDS = "wifi_creds_v1";

static esp_err_t ensure_nvs_ready(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "Erasing NVS");
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    return err;
}

void fishduino_wifi_creds_defaults(fishduino_wifi_creds_t *out)
{
    memset(out, 0, sizeof(*out));
    strncpy(out->ssid, CONFIG_ESP_WIFI_SSID, sizeof(out->ssid) - 1);
    strncpy(out->password, CONFIG_ESP_WIFI_PASSWORD, sizeof(out->password) - 1);
    out->stored = false;
}

bool fishduino_wifi_creds_load(fishduino_wifi_creds_t *out)
{
    fishduino_wifi_creds_defaults(out);

    if (ensure_nvs_ready() != ESP_OK) {
        return false;
    }

    nvs_handle_t nvh;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvh);
    if (err != ESP_OK) {
        return false;
    }

    size_t size = sizeof(*out);
    err = nvs_get_blob(nvh, NVS_KEY_WIFI_CREDS, out, &size);
    nvs_close(nvh);

    if (err != ESP_OK || size != sizeof(*out)) {
        return false;
    }

    out->ssid[sizeof(out->ssid) - 1] = '\0';
    out->password[sizeof(out->password) - 1] = '\0';
    return out->stored;
}

bool fishduino_wifi_creds_save(const fishduino_wifi_creds_t *in)
{
    if (ensure_nvs_ready() != ESP_OK) {
        return false;
    }

    fishduino_wifi_creds_t copy = *in;
    copy.stored = true;
    copy.ssid[sizeof(copy.ssid) - 1] = '\0';
    copy.password[sizeof(copy.password) - 1] = '\0';

    nvs_handle_t nvh;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvh);
    if (err != ESP_OK) {
        return false;
    }

    err = nvs_set_blob(nvh, NVS_KEY_WIFI_CREDS, &copy, sizeof(copy));
    if (err == ESP_OK) {
        err = nvs_commit(nvh);
    }
    nvs_close(nvh);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Save failed: %s", esp_err_to_name(err));
        return false;
    }

    return true;
}

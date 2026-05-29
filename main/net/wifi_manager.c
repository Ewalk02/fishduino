#include "wifi_manager.h"

#include <stdlib.h>
#include <string.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "storage/wifi_creds_nvs.h"
#include "time_sync.h"

#if !defined(CONFIG_ESP_WIFI_STATIC_RX_BUFFER_NUM)

bool fishduino_wifi_init(void)
{
    return false;
}

bool fishduino_wifi_start_sta(void)
{
    return false;
}

bool fishduino_wifi_is_connected(void)
{
    return false;
}

void fishduino_wifi_get_credentials(char *ssid, size_t ssid_len, char *password, size_t password_len)
{
    if (ssid != NULL && ssid_len > 0) {
        ssid[0] = '\0';
    }
    if (password != NULL && password_len > 0) {
        password[0] = '\0';
    }
}

void fishduino_wifi_apply_credentials_async(const char *ssid, const char *password)
{
    (void)ssid;
    (void)password;
}

const char *fishduino_wifi_status_text(void)
{
    return "Wi-Fi unavailable";
}

#else

#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"

static const char *TAG = "wifi";

static EventGroupHandle_t s_events;
static const EventBits_t WIFI_CONNECTED_BIT = BIT0;

static char s_ssid[FISHDUINO_WIFI_SSID_MAX + 1];
static char s_password[FISHDUINO_WIFI_PASS_MAX + 1];
static bool s_stack_ready = false;
static volatile bool s_connecting = false;

typedef struct {
    char ssid[FISHDUINO_WIFI_SSID_MAX + 1];
    char password[FISHDUINO_WIFI_PASS_MAX + 1];
} wifi_apply_req_t;

static void copy_credentials_to_static(const char *ssid, const char *password)
{
    strncpy(s_ssid, ssid != NULL ? ssid : "", sizeof(s_ssid) - 1);
    strncpy(s_password, password != NULL ? password : "", sizeof(s_password) - 1);
    s_ssid[sizeof(s_ssid) - 1] = '\0';
    s_password[sizeof(s_password) - 1] = '\0';
}

static bool apply_sta_config(void)
{
    wifi_config_t wifi_config = {0};
    strncpy((char *)wifi_config.sta.ssid, s_ssid, sizeof(wifi_config.sta.ssid));
    strncpy((char *)wifi_config.sta.password, s_password, sizeof(wifi_config.sta.password));
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    wifi_config.sta.pmf_cfg.capable = true;
    wifi_config.sta.pmf_cfg.required = false;

    esp_err_t err = esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "set_config failed: %s", esp_err_to_name(err));
        return false;
    }
    return true;
}

static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    (void)arg;
    (void)event_data;

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        s_connecting = true;
        esp_wifi_connect();
        return;
    }

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        xEventGroupClearBits(s_events, WIFI_CONNECTED_BIT);
        s_connecting = true;
        esp_wifi_connect();
        return;
    }

    if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        xEventGroupSetBits(s_events, WIFI_CONNECTED_BIT);
        s_connecting = false;
        fishduino_time_sync_on_wifi_connected();
        return;
    }
}

bool fishduino_wifi_init(void)
{
    fishduino_wifi_creds_t creds;
    if (fishduino_wifi_creds_load(&creds)) {
        copy_credentials_to_static(creds.ssid, creds.password);
        ESP_LOGI(TAG, "Wi-Fi creds from NVS");
    } else {
        fishduino_wifi_creds_defaults(&creds);
        copy_credentials_to_static(creds.ssid, creds.password);
        ESP_LOGI(TAG, "Wi-Fi creds from menuconfig");
    }
    return true;
}

bool fishduino_wifi_start_sta(void)
{
    if (s_events == NULL) {
        s_events = xEventGroupCreate();
        if (s_events == NULL) {
            return false;
        }
    }

    if (!s_stack_ready) {
        ESP_ERROR_CHECK(esp_netif_init());
        ESP_ERROR_CHECK(esp_event_loop_create_default());
        esp_netif_create_default_wifi_sta();

        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        ESP_ERROR_CHECK(esp_wifi_init(&cfg));

        ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
        ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL));

        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        s_stack_ready = true;
    }

    if (!apply_sta_config()) {
        return false;
    }

    if (esp_wifi_start() != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_start failed");
        return false;
    }

    s_connecting = true;
    ESP_LOGI(TAG, "Wi-Fi STA started");
    return true;
}

bool fishduino_wifi_is_connected(void)
{
    if (s_events == NULL) {
        return false;
    }
    return (xEventGroupGetBits(s_events) & WIFI_CONNECTED_BIT) != 0;
}

void fishduino_wifi_get_credentials(char *ssid, size_t ssid_len, char *password, size_t password_len)
{
    if (ssid != NULL && ssid_len > 0) {
        strncpy(ssid, s_ssid, ssid_len - 1);
        ssid[ssid_len - 1] = '\0';
    }
    if (password != NULL && password_len > 0) {
        strncpy(password, s_password, password_len - 1);
        password[password_len - 1] = '\0';
    }
}

static void wifi_apply_task(void *arg)
{
    wifi_apply_req_t *req = (wifi_apply_req_t *)arg;
    if (req == NULL) {
        vTaskDelete(NULL);
        return;
    }

    copy_credentials_to_static(req->ssid, req->password);

    fishduino_wifi_creds_t creds = {0};
    strncpy(creds.ssid, req->ssid, sizeof(creds.ssid) - 1);
    strncpy(creds.password, req->password, sizeof(creds.password) - 1);
    creds.stored = true;
    fishduino_wifi_creds_save(&creds);

    free(req);

    if (s_stack_ready) {
        xEventGroupClearBits(s_events, WIFI_CONNECTED_BIT);
        s_connecting = true;
        esp_wifi_disconnect();
        vTaskDelay(pdMS_TO_TICKS(200));
        apply_sta_config();
        esp_wifi_connect();
    } else {
        fishduino_wifi_start_sta();
    }

    ESP_LOGI(TAG, "Wi-Fi credentials updated from UI");
    vTaskDelete(NULL);
}

void fishduino_wifi_apply_credentials_async(const char *ssid, const char *password)
{
    if (ssid == NULL) {
        return;
    }

    wifi_apply_req_t *req = calloc(1, sizeof(*req));
    if (req == NULL) {
        return;
    }

    strncpy(req->ssid, ssid, sizeof(req->ssid) - 1);
    if (password != NULL) {
        strncpy(req->password, password, sizeof(req->password) - 1);
    }

    xTaskCreate(wifi_apply_task, "wifi_apply", 4096, req, 5, NULL);
}

const char *fishduino_wifi_status_text(void)
{
    if (!s_stack_ready) {
        return "Wi-Fi not started";
    }
    if (fishduino_wifi_is_connected()) {
        return "Wi-Fi connected";
    }
    if (s_connecting) {
        return "Wi-Fi connecting...";
    }
    return "Wi-Fi disconnected";
}

#endif

#include "wifi_manager.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
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

void fishduino_wifi_get_status(fishduino_wifi_status_t *out)
{
    if (out != NULL) {
        memset(out, 0, sizeof(*out));
        out->kind = FISHDUINO_WIFI_STATUS_UNAVAILABLE;
    }
}

const char *fishduino_wifi_status_text(void)
{
    return "Wi-Fi unavailable";
}

bool fishduino_wifi_get_sta_ip(char *buf, size_t len)
{
    if (buf != NULL && len > 0) {
        strncpy(buf, "unavailable", len - 1);
        buf[len - 1] = '\0';
    }
    return false;
}

#else

#include "esp_event.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "esp_wifi.h"

static const char *TAG = "wifi";

static EventGroupHandle_t s_events;
static const EventBits_t WIFI_CONNECTED_BIT = BIT0;

static char s_ssid[FISHDUINO_WIFI_SSID_MAX + 1];
static char s_password[FISHDUINO_WIFI_PASS_MAX + 1];
static bool s_stack_ready = false;
static bool s_credentials_missing = false;
static volatile bool s_connecting = false;
static uint32_t s_reconnect_attempt = 0;
static int s_last_disconnect_reason = 0;
static char s_reason_text[64] = "";
static esp_timer_handle_t s_reconnect_timer = NULL;
static uint32_t s_backoff_ms = 1000;

typedef struct {
    char ssid[FISHDUINO_WIFI_SSID_MAX + 1];
    char password[FISHDUINO_WIFI_PASS_MAX + 1];
} wifi_apply_req_t;

static void reason_to_text(int reason, char *buf, size_t len)
{
    const char *name = "unknown";
    switch (reason) {
    case WIFI_REASON_UNSPECIFIED:
        name = "unspecified";
        break;
    case WIFI_REASON_AUTH_EXPIRE:
        name = "auth_expire";
        break;
    case WIFI_REASON_AUTH_LEAVE:
        name = "auth_leave";
        break;
    case WIFI_REASON_ASSOC_EXPIRE:
        name = "assoc_expire";
        break;
    case WIFI_REASON_ASSOC_TOOMANY:
        name = "assoc_toomany";
        break;
    case WIFI_REASON_NOT_AUTHED:
        name = "not_authed";
        break;
    case WIFI_REASON_NOT_ASSOCED:
        name = "not_assoced";
        break;
    case WIFI_REASON_ASSOC_LEAVE:
        name = "assoc_leave";
        break;
    case WIFI_REASON_ASSOC_NOT_AUTHED:
        name = "assoc_not_authed";
        break;
    case WIFI_REASON_DISASSOC_PWRCAP_BAD:
        name = "disassoc_pwrcap";
        break;
    case WIFI_REASON_DISASSOC_SUPCHAN_BAD:
        name = "disassoc_supchan";
        break;
    case WIFI_REASON_IE_INVALID:
        name = "ie_invalid";
        break;
    case WIFI_REASON_MIC_FAILURE:
        name = "mic_failure";
        break;
    case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT:
        name = "4way_timeout";
        break;
    case WIFI_REASON_GROUP_KEY_UPDATE_TIMEOUT:
        name = "group_key_timeout";
        break;
    case WIFI_REASON_BEACON_TIMEOUT:
        name = "beacon_timeout";
        break;
    case WIFI_REASON_NO_AP_FOUND:
        name = "no_ap_found";
        break;
    case WIFI_REASON_AUTH_FAIL:
        name = "auth_fail";
        break;
    case WIFI_REASON_ASSOC_FAIL:
        name = "assoc_fail";
        break;
    case WIFI_REASON_HANDSHAKE_TIMEOUT:
        name = "handshake_timeout";
        break;
    case WIFI_REASON_CONNECTION_FAIL:
        name = "connection_fail";
        break;
    default:
        break;
    }
    snprintf(buf, len, "%s (%d)", name, reason);
}

static void copy_credentials_to_static(const char *ssid, const char *password)
{
    strncpy(s_ssid, ssid != NULL ? ssid : "", sizeof(s_ssid) - 1);
    strncpy(s_password, password != NULL ? password : "", sizeof(s_password) - 1);
    s_ssid[sizeof(s_ssid) - 1] = '\0';
    s_password[sizeof(s_password) - 1] = '\0';
    s_credentials_missing = (s_ssid[0] == '\0');
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

static void schedule_reconnect(void);

static void reconnect_timer_cb(void *arg)
{
    (void)arg;
    if (s_credentials_missing || !s_stack_ready) {
        return;
    }
    if (fishduino_wifi_is_connected()) {
        return;
    }

    s_reconnect_attempt++;
    s_connecting = true;
    ESP_LOGI(TAG, "Wi-Fi reconnect attempt %lu (backoff %lu ms)", (unsigned long)s_reconnect_attempt,
             (unsigned long)s_backoff_ms);
    esp_wifi_connect();
}

static void schedule_reconnect(void)
{
    if (s_credentials_missing) {
        return;
    }

    if (s_reconnect_timer == NULL) {
        const esp_timer_create_args_t args = {
            .callback = &reconnect_timer_cb,
            .name = "wifi_reconn",
        };
        if (esp_timer_create(&args, &s_reconnect_timer) != ESP_OK) {
            esp_wifi_connect();
            return;
        }
    }

    esp_timer_stop(s_reconnect_timer);
    esp_timer_start_once(s_reconnect_timer, (uint64_t)s_backoff_ms * 1000ULL);

    if (s_backoff_ms < 60000) {
        s_backoff_ms *= 2;
        if (s_backoff_ms > 60000) {
            s_backoff_ms = 60000;
        }
    }
}

static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    (void)arg;

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        if (s_credentials_missing) {
            ESP_LOGW(TAG, "Wi-Fi STA started but credentials missing");
            return;
        }
        s_connecting = true;
        s_backoff_ms = 1000;
        esp_wifi_connect();
        return;
    }

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        const wifi_event_sta_disconnected_t *disc = (const wifi_event_sta_disconnected_t *)event_data;
        xEventGroupClearBits(s_events, WIFI_CONNECTED_BIT);
        s_connecting = false;

        if (disc != NULL) {
            s_last_disconnect_reason = disc->reason;
            reason_to_text(disc->reason, s_reason_text, sizeof(s_reason_text));
            ESP_LOGW(TAG, "Wi-Fi disconnected: %s", s_reason_text);
        }

        schedule_reconnect();
        return;
    }

    if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        xEventGroupSetBits(s_events, WIFI_CONNECTED_BIT);
        s_connecting = false;
        s_reconnect_attempt = 0;
        s_backoff_ms = 1000;
        s_last_disconnect_reason = 0;
        s_reason_text[0] = '\0';
        if (s_reconnect_timer != NULL) {
            esp_timer_stop(s_reconnect_timer);
        }
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
    if (s_credentials_missing) {
        ESP_LOGW(TAG, "Wi-Fi credentials missing; not starting STA");
        return false;
    }

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
    s_reconnect_attempt = 0;
    s_backoff_ms = 1000;
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

    s_reconnect_attempt = 0;
    s_backoff_ms = 1000;

    if (s_stack_ready) {
        xEventGroupClearBits(s_events, WIFI_CONNECTED_BIT);
        s_connecting = true;
        esp_wifi_disconnect();
        vTaskDelay(pdMS_TO_TICKS(200));
        if (!s_credentials_missing) {
            apply_sta_config();
            esp_wifi_connect();
        }
    } else if (!s_credentials_missing) {
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

void fishduino_wifi_get_status(fishduino_wifi_status_t *out)
{
    if (out == NULL) {
        return;
    }
    memset(out, 0, sizeof(*out));
    out->last_disconnect_reason = s_last_disconnect_reason;
    strncpy(out->reason_text, s_reason_text, sizeof(out->reason_text) - 1);
    out->reconnect_attempt = s_reconnect_attempt;

    if (s_credentials_missing) {
        out->kind = FISHDUINO_WIFI_STATUS_CREDENTIALS_MISSING;
        return;
    }
    if (!s_stack_ready) {
        out->kind = FISHDUINO_WIFI_STATUS_NOT_STARTED;
        return;
    }
    if (fishduino_wifi_is_connected()) {
        out->kind = FISHDUINO_WIFI_STATUS_CONNECTED;
        return;
    }
    if (s_connecting) {
        out->kind = FISHDUINO_WIFI_STATUS_CONNECTING;
        return;
    }
    out->kind = FISHDUINO_WIFI_STATUS_DISCONNECTED;
}

const char *fishduino_wifi_status_text(void)
{
    fishduino_wifi_status_t st;
    fishduino_wifi_get_status(&st);

    static char buf[128];
    switch (st.kind) {
    case FISHDUINO_WIFI_STATUS_CREDENTIALS_MISSING:
        return "Wi-Fi credentials missing";
    case FISHDUINO_WIFI_STATUS_NOT_STARTED:
        return "Wi-Fi not started";
    case FISHDUINO_WIFI_STATUS_CONNECTED:
        return "Wi-Fi connected";
    case FISHDUINO_WIFI_STATUS_CONNECTING:
        snprintf(buf, sizeof(buf), "Wi-Fi connecting (attempt %lu)", (unsigned long)st.reconnect_attempt);
        return buf;
    case FISHDUINO_WIFI_STATUS_DISCONNECTED:
        if (st.reason_text[0] != '\0') {
            snprintf(buf, sizeof(buf), "Wi-Fi disconnected #%lu: %s", (unsigned long)st.reconnect_attempt,
                     st.reason_text);
        } else {
            snprintf(buf, sizeof(buf), "Wi-Fi disconnected (attempt %lu)", (unsigned long)st.reconnect_attempt);
        }
        return buf;
    default:
        return "Wi-Fi unavailable";
    }
}

bool fishduino_wifi_get_sta_ip(char *buf, size_t len)
{
    if (buf == NULL || len == 0) {
        return false;
    }

    if (!fishduino_wifi_is_connected()) {
        strncpy(buf, "not connected", len - 1);
        buf[len - 1] = '\0';
        return false;
    }

    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (netif == NULL) {
        strncpy(buf, "no netif", len - 1);
        buf[len - 1] = '\0';
        return false;
    }

    esp_netif_ip_info_t info;
    if (esp_netif_get_ip_info(netif, &info) != ESP_OK || info.ip.addr == 0) {
        strncpy(buf, "no IP", len - 1);
        buf[len - 1] = '\0';
        return false;
    }

    snprintf(buf, len, IPSTR, IP2STR(&info.ip));
    return true;
}

#endif

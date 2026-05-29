#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    FISHDUINO_WIFI_STATUS_UNAVAILABLE = 0,
    FISHDUINO_WIFI_STATUS_CREDENTIALS_MISSING,
    FISHDUINO_WIFI_STATUS_NOT_STARTED,
    FISHDUINO_WIFI_STATUS_CONNECTING,
    FISHDUINO_WIFI_STATUS_CONNECTED,
    FISHDUINO_WIFI_STATUS_DISCONNECTED,
} fishduino_wifi_status_kind_t;

typedef struct {
    fishduino_wifi_status_kind_t kind;
    uint32_t reconnect_attempt;
    int last_disconnect_reason;
    char reason_text[64];
} fishduino_wifi_status_t;

bool fishduino_wifi_init(void);
bool fishduino_wifi_start_sta(void);
bool fishduino_wifi_is_connected(void);

void fishduino_wifi_get_credentials(char *ssid, size_t ssid_len, char *password, size_t password_len);

/** Save to NVS and reconnect (safe to call from UI; work runs in background task). */
void fishduino_wifi_apply_credentials_async(const char *ssid, const char *password);

void fishduino_wifi_get_status(fishduino_wifi_status_t *out);
const char *fishduino_wifi_status_text(void);

/** STA IPv4 string e.g. "192.168.1.50", or "not connected". */
bool fishduino_wifi_get_sta_ip(char *buf, size_t len);

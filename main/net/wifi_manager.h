#pragma once

#include <stdbool.h>
#include <stddef.h>

bool fishduino_wifi_init(void);
bool fishduino_wifi_start_sta(void);
bool fishduino_wifi_is_connected(void);

void fishduino_wifi_get_credentials(char *ssid, size_t ssid_len, char *password, size_t password_len);

/** Save to NVS and reconnect (safe to call from UI; work runs in background task). */
void fishduino_wifi_apply_credentials_async(const char *ssid, const char *password);

const char *fishduino_wifi_status_text(void);

#pragma once

#include <stdbool.h>
#include <stddef.h>

#define FISHDUINO_WIFI_SSID_MAX 32
#define FISHDUINO_WIFI_PASS_MAX 64

typedef struct {
    char ssid[FISHDUINO_WIFI_SSID_MAX + 1];
    char password[FISHDUINO_WIFI_PASS_MAX + 1];
    bool stored;
} fishduino_wifi_creds_t;

void fishduino_wifi_creds_defaults(fishduino_wifi_creds_t *out);
bool fishduino_wifi_creds_load(fishduino_wifi_creds_t *out);
bool fishduino_wifi_creds_save(const fishduino_wifi_creds_t *in);

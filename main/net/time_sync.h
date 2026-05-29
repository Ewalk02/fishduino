#pragma once

#include <stdbool.h>

#include "storage/settings_nvs.h"

void fishduino_time_sync_init(void);
void fishduino_time_sync_apply_timezone(const fishduino_settings_t *settings);
void fishduino_time_sync_on_wifi_connected(void);
bool fishduino_time_sync_is_done(void);

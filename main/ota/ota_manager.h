#pragma once

#include <stdbool.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    FISHDUINO_OTA_IDLE = 0,
    FISHDUINO_OTA_DOWNLOADING,
    FISHDUINO_OTA_VALIDATING,
    FISHDUINO_OTA_PENDING_REBOOT,
    FISHDUINO_OTA_SUCCESS,
    FISHDUINO_OTA_FAILED,
} fishduino_ota_state_t;

void fishduino_ota_manager_init(void);
void fishduino_ota_manager_tick(void);

fishduino_ota_state_t fishduino_ota_manager_get_state(void);
const char *fishduino_ota_manager_state_text(fishduino_ota_state_t st);
const char *fishduino_ota_get_version_string(void);

esp_err_t fishduino_ota_start_url(const char *url);
esp_err_t fishduino_ota_confirm_good(void);

#ifdef __cplusplus
}
#endif

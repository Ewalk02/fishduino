#pragma once

#include <stdbool.h>

#include "esp_err.h"
#include "fluval_protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

#define FLUVAL_TARGET_NAME "Plant4.0_450467"

#define FLUVAL_POLL_INTERVAL_MS    10000
#define FLUVAL_STALE_TIMEOUT_MS    30000
#define FLUVAL_CMD_WAIT_MS           3000

esp_err_t fluval_ble_init(void);
esp_err_t fluval_ble_start(void);

bool fluval_ble_is_connected(void);

esp_err_t fluval_ble_get_state(fluval_state_t *out);

esp_err_t fluval_ble_request_status(void);
esp_err_t fluval_ble_wait_status(fluval_state_t *out, uint32_t timeout_ms);

esp_err_t fluval_ble_set_mode_manual(void);
esp_err_t fluval_ble_set_mode_auto(void);
esp_err_t fluval_ble_set_channels(uint8_t pink, uint8_t blue, uint8_t cold_white, uint8_t white,
                                  uint8_t warm_white);
esp_err_t fluval_ble_set_all(uint8_t percent);

#ifdef __cplusplus
}
#endif

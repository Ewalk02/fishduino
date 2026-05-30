#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "fluval_protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

#define FLUVAL_BLE_NOTIFY_MAX_LEN 256
#define FLUVAL_BLE_CMD_WAIT_MS      3000

typedef struct {
    fluval_protocol_status_t protocol;
    int rssi;
    uint32_t last_update_ms;
} fishduino_fluval_ble_client_state_t;

typedef struct {
    char target_name[32];
    uint32_t poll_interval_ms;
    uint32_t stale_timeout_ms;
} fishduino_fluval_ble_client_config_t;

esp_err_t fishduino_fluval_ble_client_init(void);
esp_err_t fishduino_fluval_ble_client_start(void);
esp_err_t fishduino_fluval_ble_client_set_config(const fishduino_fluval_ble_client_config_t *cfg);

bool fishduino_fluval_ble_client_is_connected(void);

esp_err_t fishduino_fluval_ble_client_get_state(fishduino_fluval_ble_client_state_t *out);

esp_err_t fishduino_fluval_ble_client_request_status(void);
esp_err_t fishduino_fluval_ble_client_wait_status(fishduino_fluval_ble_client_state_t *out, uint32_t timeout_ms);

esp_err_t fishduino_fluval_ble_client_set_mode_manual(void);
esp_err_t fishduino_fluval_ble_client_set_mode_auto(void);
esp_err_t fishduino_fluval_ble_client_set_channels(uint8_t pink, uint8_t blue, uint8_t cold_white, uint8_t white,
                                                   uint8_t warm_white);
esp_err_t fishduino_fluval_ble_client_set_all(uint8_t percent);

#ifdef __cplusplus
}
#endif

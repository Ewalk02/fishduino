#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "chihiros_heater_protocol.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    bool connected;
    bool subscribed;
    bool stale;
    chihiros_status_t last_status;
    uint32_t last_status_ms;
    float last_setpoint_f;
    bool has_last_setpoint;
    int rssi;
} chihiros_ble_client_state_t;

typedef struct {
    char name_prefix[8];
    uint32_t stale_timeout_ms;
    float min_setpoint_f;
    float max_setpoint_f;
} chihiros_ble_client_config_t;

esp_err_t chihiros_ble_client_init(void);
esp_err_t chihiros_ble_client_set_config(const chihiros_ble_client_config_t *cfg);
esp_err_t chihiros_ble_client_set_enabled(bool enabled);
void chihiros_ble_client_request_connect(void);

bool chihiros_ble_client_is_ready(void);
esp_err_t chihiros_ble_client_get_state(chihiros_ble_client_state_t *out);
bool chihiros_ble_client_set_setpoint_f(float target_f);

#ifdef __cplusplus
}
#endif

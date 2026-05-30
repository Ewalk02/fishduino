#pragma once

#include <stdint.h>

#include "esp_err.h"
#include "fluval_transport.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t fishduino_fluval_ble_transport_set_callback(fishduino_fluval_transport_line_cb_t cb, void *ctx);
esp_err_t fishduino_fluval_ble_transport_apply_config(const char *target_name, uint16_t poll_interval_s,
                                                      uint16_t stale_timeout_s);

#ifdef __cplusplus
}
#endif

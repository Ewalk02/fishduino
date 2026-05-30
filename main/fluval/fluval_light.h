#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    FISHDUINO_FLUVAL_MODE_UNKNOWN = 0,
    FISHDUINO_FLUVAL_MODE_MANUAL,
    FISHDUINO_FLUVAL_MODE_AUTO,
} fishduino_fluval_mode_t;

typedef enum {
    FISHDUINO_FLUVAL_LINK_DISABLED = 0,
    FISHDUINO_FLUVAL_LINK_OFFLINE,
    FISHDUINO_FLUVAL_LINK_STALE,
    FISHDUINO_FLUVAL_LINK_OK,
} fishduino_fluval_link_t;

typedef struct {
    bool connected;
    bool stale;
    fishduino_fluval_mode_t mode;
    uint8_t pink;
    uint8_t blue;
    uint8_t cold_white;
    uint8_t white;
    uint8_t warm_white;
    uint8_t avg_output;
    int rssi;
    uint32_t last_update_ms;
} fishduino_fluval_state_t;

esp_err_t fishduino_fluval_init(void);
esp_err_t fishduino_fluval_start(void);
esp_err_t fishduino_fluval_tick(void);

esp_err_t fishduino_fluval_get_state(fishduino_fluval_state_t *out);
fishduino_fluval_link_t fishduino_fluval_get_link_status(void);
const char *fishduino_fluval_mode_text(fishduino_fluval_mode_t mode);
const char *fishduino_fluval_link_text(fishduino_fluval_link_t link);

esp_err_t fishduino_fluval_request_status(void);
esp_err_t fishduino_fluval_set_mode_manual(void);
esp_err_t fishduino_fluval_set_mode_auto(void);
esp_err_t fishduino_fluval_set_channels(uint8_t pink, uint8_t blue, uint8_t cold_white, uint8_t white,
                                        uint8_t warm_white);
esp_err_t fishduino_fluval_set_all(uint8_t percent);

#ifdef __cplusplus
}
#endif

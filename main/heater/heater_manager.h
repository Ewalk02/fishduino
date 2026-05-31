#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    HEATER_STATE_UNKNOWN = 0,
    HEATER_STATE_OFFLINE,
    HEATER_STATE_CONNECTING,
    HEATER_STATE_ON,
    HEATER_STATE_OFF,
    HEATER_STATE_ERROR,
} heater_state_t;

typedef enum {
    HEATER_ALARM_NONE = 0,
    HEATER_ALARM_OFFLINE,
    HEATER_ALARM_STALE,
    HEATER_ALARM_OVER_TEMP,
    HEATER_ALARM_UNDER_TEMP,
    HEATER_ALARM_COMMAND_FAILED,
} heater_alarm_t;

typedef struct {
    heater_state_t state;
    float reported_temp_f;
    float target_temp_f;
    bool heating;
    bool online;
    bool stale;
    bool enabled;
    int rssi;
    int64_t last_seen_ms;
    heater_alarm_t alarm;
    char error_text[96];
} heater_status_t;

esp_err_t heater_manager_init(void);
void heater_manager_tick(void);

esp_err_t heater_manager_set_enabled(bool enabled);
esp_err_t heater_manager_set_target_temp_f(float target_f);
esp_err_t heater_manager_get_status(heater_status_t *out);

const char *heater_manager_state_text(heater_state_t state);
const char *heater_manager_alarm_text(heater_alarm_t alarm);

#ifdef __cplusplus
}
#endif

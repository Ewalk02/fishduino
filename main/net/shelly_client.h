#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "storage/settings_nvs.h"

/*
 * Blocking HTTP RPC — call only from shelly_task (never under bsp_display_lock).
 *
 * GET http://<ip>/rpc/Switch.GetStatus?id=0
 * GET http://<ip>/rpc/Switch.Set?id=0&on=true|false  (CO2 plug only via co2_set_output)
 */

typedef struct {
    bool online;
    bool output;
    float watts;
    float voltage;
    float current;
    float power_factor;
    float frequency;
    float energy_wh;
    float temperature_f;
    char error_text[64];
    uint32_t last_success_ms;
    uint8_t fail_count;
} fishduino_shelly_switch_status_t;

bool fishduino_shelly_get_switch_status(const char *ip, int switch_id, fishduino_shelly_switch_status_t *out);

/** Switch.Set — CO2 plug only; refuses filter IP. */
bool fishduino_shelly_co2_set_output(const fishduino_settings_t *settings, bool on);

/** Switch.Set — heater plug only; refuses filter IP. */
bool fishduino_shelly_heater_set_output(const fishduino_settings_t *settings, bool on);

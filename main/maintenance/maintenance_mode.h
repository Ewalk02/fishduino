#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

void fishduino_maintenance_mode_init(void);
void fishduino_maintenance_mode_tick(void);

esp_err_t fishduino_maintenance_mode_start(uint32_t duration_minutes);
esp_err_t fishduino_maintenance_mode_end(void);

bool fishduino_maintenance_mode_is_active(void);
int64_t fishduino_maintenance_mode_remaining_ms(void);

/** When true, filter OFF/LOW_POWER dashboard alarms are suppressed. */
bool fishduino_maintenance_mode_suppress_filter_alarms(void);

#ifdef __cplusplus
}
#endif

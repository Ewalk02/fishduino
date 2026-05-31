#pragma once

#include <stdbool.h>

#include "storage/settings_nvs.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    CO2_BLOCK_NONE = 0,
    CO2_BLOCK_MAINTENANCE,
    CO2_BLOCK_CO2_OFFLINE,
    CO2_BLOCK_FILTER_DISABLED,
    CO2_BLOCK_FILTER_OFFLINE,
    CO2_BLOCK_FILTER_STALE,
    CO2_BLOCK_FILTER_UNCALIBRATED,
    CO2_BLOCK_FILTER_OFF,
    CO2_BLOCK_FILTER_LOW_POWER,
    CO2_BLOCK_FILTER_BELOW_THRESHOLD,
    CO2_BLOCK_NO_TIME_SYNC,
    CO2_BLOCK_GLOBAL_LOCKOUT,
    CO2_BLOCK_CO2_COMMAND_FAILED,
} co2_safety_reason_t;

void fishduino_co2_safety_init(void);

/** True only when all interlocks allow energizing CO2. */
bool fishduino_co2_safety_allows_on(co2_safety_reason_t *reason);

/**
 * Apply interlock to a desired ON state (schedule/manual target).
 * Returns effective output (fail-safe OFF when blocked).
 */
bool fishduino_co2_safety_effective_desired_on(bool wants_on, co2_safety_reason_t *reason);

const char *fishduino_co2_safety_reason_text(co2_safety_reason_t reason);

void fishduino_co2_safety_set_last_block(co2_safety_reason_t reason);
co2_safety_reason_t fishduino_co2_safety_get_last_block(void);

/** Dangerous service override; auto-expires. */
bool fishduino_co2_safety_override_active(void);
void fishduino_co2_safety_override_start(uint32_t duration_minutes);
void fishduino_co2_safety_override_clear(void);

#ifdef __cplusplus
}
#endif

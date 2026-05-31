#pragma once

#include <stdbool.h>

#include "heater/heater_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

bool heater_safety_allows_heating(const heater_status_t *st, float target_f, char *err_out, size_t err_len);

#ifdef __cplusplus
}
#endif

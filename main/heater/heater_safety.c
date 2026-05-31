#include "heater_safety.h"

#include <stdio.h>

#include "storage/settings_runtime.h"

bool heater_safety_allows_heating(const heater_status_t *st, float target_f, char *err_out, size_t err_len)
{
    if (st == NULL) {
        if (err_out && err_len > 0) {
            snprintf(err_out, err_len, "no status");
        }
        return false;
    }

    if (!st->enabled) {
        if (err_out && err_len > 0) {
            snprintf(err_out, err_len, "heater disabled");
        }
        return false;
    }

    if (!st->online || st->stale) {
        if (err_out && err_len > 0) {
            snprintf(err_out, err_len, "heater offline or stale");
        }
        return false;
    }

    if (st->reported_temp_f <= 0.0f) {
        if (err_out && err_len > 0) {
            snprintf(err_out, err_len, "temperature unknown");
        }
        return false;
    }

    fishduino_settings_t settings;
    if (fishduino_settings_get_snapshot(&settings)) {
        float max_over = settings.heater.max_over_target_f;
        if (max_over <= 0.0f) {
            max_over = 3.0f;
        }
        if (st->reported_temp_f > target_f + max_over) {
            if (err_out && err_len > 0) {
                snprintf(err_out, err_len, "over temp limit");
            }
            return false;
        }
    }

    (void)target_f;
    return true;
}

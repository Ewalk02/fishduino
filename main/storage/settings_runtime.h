#pragma once

#include <stdbool.h>

#include "storage/settings_nvs.h"

/** Load NVS once at boot; creates settings mutex. */
void fishduino_settings_runtime_init(void);

/** Thread-safe copy of current settings. */
bool fishduino_settings_get_snapshot(fishduino_settings_t *out);

typedef bool (*fishduino_settings_mutator_fn)(fishduino_settings_t *st, void *ctx);

/**
 * Apply mutator under mutex. If persist is true, writes NVS after successful mutator.
 * Mutator returns false to abort without saving.
 */
bool fishduino_settings_update(fishduino_settings_mutator_fn fn, void *ctx, bool persist);

#pragma once

#include <stddef.h>

#include "esp_err.h"
#include "water/water_metrics.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t water_storage_init(void);

esp_err_t water_storage_append(const water_test_entry_t *entry);

esp_err_t water_storage_get_latest(water_test_entry_t *out);

size_t water_storage_count(void);

esp_err_t water_storage_get_entries(water_test_entry_t *out, size_t max_entries, size_t *out_count);

esp_err_t water_storage_clear_all(void);

#ifdef __cplusplus
}
#endif

#pragma once

#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define WATER_TEST_NOTES_LEN 96
#define WATER_METRICS_MAX_ENTRIES 200

#define WATER_VALID_PH          (1U << 0)
#define WATER_VALID_AMMONIA     (1U << 1)
#define WATER_VALID_NITRITE     (1U << 2)
#define WATER_VALID_NITRATE     (1U << 3)
#define WATER_VALID_NOTES       (1U << 4)
#define WATER_FLAG_MANUAL       (1U << 5)
#define WATER_FLAG_TIME_UNKNOWN (1U << 6)

typedef struct {
    int64_t timestamp_unix;
    float ph;
    float ammonia_ppm;
    float nitrite_ppm;
    float nitrate_ppm;
    char notes[WATER_TEST_NOTES_LEN];
    uint8_t valid_flags;
} water_test_entry_t;

esp_err_t water_metrics_init(void);

esp_err_t water_metrics_add_entry(const water_test_entry_t *entry);

esp_err_t water_metrics_get_latest(water_test_entry_t *out);

size_t water_metrics_count(void);

esp_err_t water_metrics_get_entries(water_test_entry_t *out, size_t max_entries, size_t *out_count);

esp_err_t water_metrics_clear_all(void);

#ifdef __cplusplus
}
#endif

#pragma once

#include <stddef.h>

#include "water/water_metrics.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    WATER_ALERT_OK = 0,
    WATER_ALERT_NOTICE,
    WATER_ALERT_WARNING,
} water_alert_level_t;

#define WATER_ALERT_PH_MIN_DEFAULT  6.0f
#define WATER_ALERT_PH_MAX_DEFAULT  8.0f
#define WATER_ALERT_NITRATE_MAX_PPM 40.0f

water_alert_level_t water_alerts_classify(const water_test_entry_t *e, char *msg, size_t msg_len);

const char *water_alerts_level_text(water_alert_level_t level);

#ifdef __cplusplus
}
#endif

#pragma once

#include "co2/co2_schedule.h"
#include "feeder/feeder_schedule.h"
#include "storage/settings_nvs.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Snapshot inputs used by dashboard_data_refresh (console / headless validation). */
void fishduino_app_dashboard_inputs(fishduino_co2_t *co2, fishduino_feeder_t *feeder,
                                    fishduino_settings_t *settings);

#ifdef __cplusplus
}
#endif

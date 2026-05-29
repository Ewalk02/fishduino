#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "scheduler/scheduler.h"
#include "storage/settings_nvs.h"

typedef struct {
    fishduino_settings_t settings;
    bool output_on;
} fishduino_co2_t;

void fishduino_co2_init(fishduino_co2_t *co2, const fishduino_settings_t *settings);
void fishduino_co2_apply_settings(fishduino_co2_t *co2, const fishduino_settings_t *settings);
void fishduino_co2_tick(fishduino_co2_t *co2, const fishduino_time_snapshot_t *now);
bool fishduino_co2_get_output(const fishduino_co2_t *co2);
bool fishduino_co2_get_target(const fishduino_co2_t *co2, const fishduino_time_snapshot_t *now);
bool fishduino_co2_in_schedule_window(uint16_t now_min, uint16_t on_min, uint16_t off_min);


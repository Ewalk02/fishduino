#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "scheduler/scheduler.h"
#include "storage/settings_nvs.h"

typedef struct {
    fishduino_settings_t settings;
    uint16_t last_fire_min;  // 0xFFFF = never
} fishduino_feeder_t;

void fishduino_feeder_init(fishduino_feeder_t *feeder, const fishduino_settings_t *settings);
void fishduino_feeder_apply_settings(fishduino_feeder_t *feeder, const fishduino_settings_t *settings);
void fishduino_feeder_tick(fishduino_feeder_t *feeder, const fishduino_time_snapshot_t *now);
void fishduino_feeder_feed_now(fishduino_feeder_t *feeder);


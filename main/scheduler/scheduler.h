#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef uint16_t fishduino_minutes_t;

typedef struct {
    fishduino_minutes_t minutes_since_midnight;
    uint32_t epoch_seconds;
    bool valid_time;
} fishduino_time_snapshot_t;

typedef void (*fishduino_scheduler_tick_fn)(const fishduino_time_snapshot_t *now, void *ctx);

void fishduino_time_snapshot_now(fishduino_time_snapshot_t *out);

void fishduino_scheduler_start(fishduino_scheduler_tick_fn tick_cb, void *tick_ctx);


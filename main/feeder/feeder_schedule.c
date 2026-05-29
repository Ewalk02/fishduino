#include "feeder_schedule.h"

#include <string.h>

#include "feeder_actuator.h"

static bool is_enabled_time(uint16_t min)
{
    return min != 0xFFFF;
}

static bool should_fire(const fishduino_time_snapshot_t *now, uint16_t fire_min, uint16_t last_fire_min)
{
    if (!now->valid_time || !is_enabled_time(fire_min)) {
        return false;
    }
    if (now->minutes_since_midnight != fire_min) {
        return false;
    }
    return last_fire_min != fire_min;
}

void fishduino_feeder_init(fishduino_feeder_t *feeder, const fishduino_settings_t *settings)
{
    memset(feeder, 0, sizeof(*feeder));
    feeder->settings = *settings;
    feeder->last_fire_min = 0xFFFF;

    fishduino_feeder_actuator_init();
}

void fishduino_feeder_apply_settings(fishduino_feeder_t *feeder, const fishduino_settings_t *settings)
{
    feeder->settings.feeder = settings->feeder;
}

void fishduino_feeder_tick(fishduino_feeder_t *feeder, const fishduino_time_snapshot_t *now)
{
    if (should_fire(now, feeder->settings.feeder.feed_min_1, feeder->last_fire_min)) {
        feeder->last_fire_min = feeder->settings.feeder.feed_min_1;
        fishduino_feeder_actuator_pulse(feeder->settings.feeder.pulse_ms);
        return;
    }

    if (should_fire(now, feeder->settings.feeder.feed_min_2, feeder->last_fire_min)) {
        feeder->last_fire_min = feeder->settings.feeder.feed_min_2;
        fishduino_feeder_actuator_pulse(feeder->settings.feeder.pulse_ms);
        return;
    }

    if (now->valid_time && now->minutes_since_midnight != feeder->last_fire_min) {
        feeder->last_fire_min = 0xFFFF;
    }
}

void fishduino_feeder_feed_now(fishduino_feeder_t *feeder)
{
    fishduino_feeder_actuator_pulse(feeder->settings.feeder.pulse_ms);
}


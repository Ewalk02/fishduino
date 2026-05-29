#pragma once

#include <stdbool.h>
#include <stdint.h>

#define FISHDUINO_IP_LEN 16

typedef enum {
    FISHDUINO_TZ_US_EASTERN = 0,
    FISHDUINO_TZ_US_CENTRAL,
    FISHDUINO_TZ_US_MOUNTAIN,
    FISHDUINO_TZ_US_PACIFIC,
} fishduino_timezone_t;

typedef struct {
    bool enabled;
    char ip[FISHDUINO_IP_LEN];
    int8_t switch_id;
} fishduino_shelly_plug_settings_t;

typedef struct {
    bool enabled;
    uint16_t on_min;   // minutes since midnight
    uint16_t off_min;  // minutes since midnight
    bool manual_override;
    bool manual_on;
} fishduino_co2_settings_t;

typedef struct {
    uint16_t feed_min_1;     // minutes since midnight, 0xFFFF = disabled
    uint16_t feed_min_2;     // minutes since midnight, 0xFFFF = disabled
    uint32_t pulse_ms;
} fishduino_feeder_settings_t;

typedef struct {
    fishduino_co2_settings_t co2;
    fishduino_feeder_settings_t feeder;
    fishduino_shelly_plug_settings_t shelly_co2;
    fishduino_shelly_plug_settings_t shelly_filter;
    float filter_running_watts_threshold;
    uint16_t filter_low_power_alarm_delay_s;
    uint16_t co2_command_min_interval_s;
    fishduino_timezone_t timezone;
} fishduino_settings_t;

const char *fishduino_timezone_name(fishduino_timezone_t tz);

void fishduino_settings_defaults(fishduino_settings_t *out);
bool fishduino_settings_load(fishduino_settings_t *out);
bool fishduino_settings_save(const fishduino_settings_t *in);

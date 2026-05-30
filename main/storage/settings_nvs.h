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

#define FISHDUINO_FLUVAL_NAME_LEN 32
#define FISHDUINO_FLUVAL_MAC_LEN  18

typedef struct {
    uint8_t pink;
    uint8_t blue;
    uint8_t cold_white;
    uint8_t white;
    uint8_t warm_white;
} fishduino_fluval_recipe_t;

typedef enum {
    FISHDUINO_FLUVAL_TRANSPORT_DISABLED = 0,
    FISHDUINO_FLUVAL_TRANSPORT_HOSTED_BLE,
    FISHDUINO_FLUVAL_TRANSPORT_UART,
} fishduino_fluval_transport_mode_t;

typedef struct {
    bool enabled;
    fishduino_fluval_transport_mode_t transport_mode;
    char target_name[FISHDUINO_FLUVAL_NAME_LEN];
    char target_mac[FISHDUINO_FLUVAL_MAC_LEN];
    uint16_t poll_interval_s;
    uint16_t stale_timeout_s;
    fishduino_fluval_recipe_t manual_recipe;
} fishduino_fluval_settings_t;

typedef struct {
    fishduino_co2_settings_t co2;
    fishduino_feeder_settings_t feeder;
    fishduino_shelly_plug_settings_t shelly_co2;
    fishduino_shelly_plug_settings_t shelly_filter;
    float filter_running_watts_threshold;
    uint16_t filter_low_power_alarm_delay_s;
    uint16_t co2_command_min_interval_s;
    fishduino_timezone_t timezone;
    float filter_baseline_watts; /**< 0 = uncalibrated */
    fishduino_fluval_settings_t fluval;
} fishduino_settings_t;

const char *fishduino_timezone_name(fishduino_timezone_t tz);

void fishduino_settings_defaults(fishduino_settings_t *out);
bool fishduino_settings_load(fishduino_settings_t *out);
bool fishduino_settings_save(const fishduino_settings_t *in);

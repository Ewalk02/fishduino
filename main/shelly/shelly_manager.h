#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "co2/co2_schedule.h"
#include "net/shelly_client.h"
#include "safety/co2_safety.h"
#include "scheduler/scheduler.h"
#include "storage/settings_nvs.h"

typedef enum {
    FISHDUINO_FILTER_ALARM_NONE = 0,
    FISHDUINO_FILTER_ALARM_OFF,
    FISHDUINO_FILTER_ALARM_OFFLINE,
    FISHDUINO_FILTER_ALARM_LOW_POWER,
} fishduino_filter_alarm_t;

typedef struct {
    fishduino_shelly_switch_status_t co2_status;
    fishduino_shelly_switch_status_t filter_status;
    fishduino_shelly_switch_status_t heater_status;
    fishduino_shelly_switch_status_t filter_last_known;
    uint32_t filter_last_known_age_ms;
    bool co2_desired_on;
    bool co2_last_sent_on;
    bool co2_waiting_time;
    bool co2_manual_active;
    fishduino_filter_alarm_t filter_alarm;
    bool filter_output_off_alert;
    uint32_t last_poll_ms;
    uint32_t last_co2_command_ms;
    uint32_t last_heater_command_ms;
    uint32_t last_filter_good_power_ms;
    uint32_t last_filter_output_on_ms;
    bool alert_blink_on;
    uint16_t co2_manual_last_seen_min;
    bool filter_calibrating;
    uint8_t filter_calibrate_progress_s;
    co2_safety_reason_t co2_block_reason;
} fishduino_shelly_state_t;

void fishduino_shelly_manager_init(void);

/** Thread-safe copy of Shelly manager state. */
bool fishduino_shelly_manager_get_state_snapshot(fishduino_shelly_state_t *out);

void fishduino_shelly_co2_tick(fishduino_co2_t *co2, const fishduino_time_snapshot_t *now);
void fishduino_shelly_filter_alarm_tick(void);

void fishduino_shelly_co2_manual(bool on);
void fishduino_shelly_co2_auto(void);
void fishduino_shelly_co2_command_now(bool on);

/** Queue Switch.Set on heater plug (never sent to filter plug). */
void fishduino_shelly_heater_command_now(bool on);

/** Sync heater plug power: queues set when relay should match want_on. */
void fishduino_shelly_heater_apply_power(bool want_on);

void fishduino_shelly_set_co2_schedule_enabled(bool enabled);
void fishduino_shelly_set_timezone(fishduino_timezone_t tz);

bool fishduino_shelly_poll_filter_now(void);
void fishduino_shelly_filter_calibrate_start(void);

const char *fishduino_filter_alarm_text(fishduino_filter_alarm_t alarm);

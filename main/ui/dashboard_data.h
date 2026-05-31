#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "co2/co2_schedule.h"
#include "feeder/feeder_schedule.h"
#include "fluval/fluval_light.h"
#include "heater/heater_manager.h"
#include "maint_tracker/maint_tracker.h"
#include "safety/co2_safety.h"
#include "shelly/shelly_manager.h"
#include "storage/settings_nvs.h"
#include "water/water_alerts.h"
#include "water/water_metrics.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DASHBOARD_TEMP_HISTORY_LEN 24

typedef enum {
    DASHBOARD_LED_OFF = 0,
    DASHBOARD_LED_OK,
    DASHBOARD_LED_WARN,
    DASHBOARD_LED_FAULT,
    DASHBOARD_LED_NA,
} dashboard_led_state_t;

typedef struct {
    float temp_f;
    float setpoint_f;
    bool temp_valid;
    bool temp_stale;
    bool heater_enabled;
    heater_state_t heater_state;
    bool heater_heating;
    bool heater_online;
    char heater_state_text[24];
    char heater_relay_text[32];
    char heater_shelly_text[48];
    bool heater_shelly_enabled;
    bool heater_shelly_online;
    bool heater_shelly_relay_on;
    bool heater_shelly_stale;
    float heater_shelly_last_watts;

    bool filter_enabled;
    float filter_watts;
    float filter_baseline_watts;
    float filter_threshold_watts;
    fishduino_filter_alarm_t filter_alarm;
    bool filter_online;
    char filter_health_text[20];

    bool co2_enabled;
    bool co2_on;
    bool co2_desired_on;
    bool co2_relay_on;
    bool co2_relay_known;
    bool co2_online;
    bool co2_waiting_time;
    bool co2_manual;
    co2_safety_reason_t co2_block_reason;
    char co2_state_text[16];
    char co2_block_text[48];

    bool feeder_configured;
    bool feeder_scheduled;
    uint16_t next_feed_min;
    bool next_feed_valid;
    char next_feed_text[12];

    float temp_history[DASHBOARD_TEMP_HISTORY_LEN];
    uint8_t temp_history_count;
    bool temp_trend_valid;
    float temp_trend_min_f;
    float temp_trend_max_f;

    bool water_has_entry;
    water_test_entry_t water_latest;
    water_alert_level_t water_alert;
    char water_alert_text[32];
    char water_updated_text[24];
    bool water_ph_ok;
    bool water_nh3_ok;
    bool water_no2_ok;
    bool water_no3_ok;

    size_t reminders_due_count;
    char next_reminder_name[48];
    maintenance_tracker_status_t next_reminder_status;
    bool reminders_any;

    bool time_valid;
    char clock_time[16];
    char clock_date[20];
    char mode_text[16];
    bool maintenance_mode_active;
    int64_t maintenance_mode_remaining_min;
    bool co2_schedule_enabled;
    bool co2_manual_override;

    dashboard_led_state_t led_wifi;
    dashboard_led_state_t led_shelly_co2;
    dashboard_led_state_t led_shelly_filter;
    dashboard_led_state_t led_shelly_heater;
    dashboard_led_state_t led_ble;
    dashboard_led_state_t led_feeder;
    dashboard_led_state_t led_light;
    dashboard_led_state_t led_alerts;

    fishduino_fluval_state_t fluval;
    fishduino_fluval_link_t fluval_link;
    char light_mode_label[16];
    uint8_t light_intensity_pct;
    char light_sunrise_text[12];
    char light_sunset_text[12];

    bool banner_visible;
    char banner_text[64];
    bool banner_critical;
    bool filter_calibrating;
    uint8_t filter_calibrate_progress_s;
} dashboard_snapshot_t;

void dashboard_data_refresh(dashboard_snapshot_t *out, const fishduino_co2_t *co2,
                            const fishduino_feeder_t *feeder, const fishduino_settings_t *settings);

#ifdef __cplusplus
}
#endif

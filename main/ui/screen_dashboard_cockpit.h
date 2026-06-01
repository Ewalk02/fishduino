#pragma once

#include "dashboard_data.h"
#include "cockpit_gauge.h"
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    lv_obj_t *banner;
    lv_obj_t *lbl_banner;
    lv_obj_t *lbl_brand;

    cockpit_gauge_t gauge_filter;
    cockpit_gauge_t gauge_temp;
    cockpit_gauge_t gauge_co2;
    cockpit_gauge_t gauge_feeder;

    lv_obj_t *lbl_temp_setpoint;
    lv_obj_t *lbl_heater_relay;
    lv_obj_t *lbl_heater_shelly;

    lv_obj_t *panel_trend;
    lv_obj_t *chart_temp;
    lv_chart_series_t *chart_temp_series;
    lv_obj_t *lbl_trend_min;
    lv_obj_t *lbl_trend_max;

    lv_obj_t *panel_water;
    lv_obj_t *lbl_water_ph;
    lv_obj_t *lbl_water_nh3;
    lv_obj_t *lbl_water_no2;
    lv_obj_t *lbl_water_no3;
    lv_obj_t *badge_water_ph;
    lv_obj_t *badge_water_nh3;
    lv_obj_t *badge_water_no2;
    lv_obj_t *badge_water_no3;
    lv_obj_t *lbl_water_updated;
    lv_obj_t *btn_water_view;

    lv_obj_t *panel_reminders;
    lv_obj_t *lbl_reminders_due;
    lv_obj_t *lbl_reminders_next;
    lv_obj_t *btn_reminders_view;

    lv_obj_t *panel_clock;
    lv_obj_t *lbl_clock_time;
    lv_obj_t *lbl_clock_date;
    lv_obj_t *badge_mode;

    lv_obj_t *panel_systems;
    lv_obj_t *led_wifi;
    lv_obj_t *led_co2;
    lv_obj_t *led_filter;
    lv_obj_t *led_heater;
    lv_obj_t *led_ble;
    lv_obj_t *led_feeder;
    lv_obj_t *led_light;
    lv_obj_t *led_alerts;

    lv_obj_t *panel_light;
    lv_obj_t *lbl_light_mode;
    lv_obj_t *bar_light;
    lv_obj_t *lbl_light_sunrise;
    lv_obj_t *lbl_light_sunset;

    lv_obj_t *nav_bar;
    lv_obj_t *nav_btns[6];
} fishduino_cockpit_handles_t;

void fishduino_cockpit_dashboard_build(lv_obj_t *parent, fishduino_cockpit_handles_t *out);
void fishduino_cockpit_dashboard_update(fishduino_cockpit_handles_t *h, const dashboard_snapshot_t *snap);

#ifdef __cplusplus
}
#endif

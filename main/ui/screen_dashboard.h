#pragma once

#include "lvgl.h"

typedef struct {
    lv_obj_t *label_co2;
    lv_obj_t *label_co2_detail;
    lv_obj_t *label_feeder;
    lv_obj_t *label_filter;
    lv_obj_t *label_filter_energy;
    lv_obj_t *label_filter_alarm;
    lv_obj_t *label_fluval_title;
    lv_obj_t *label_fluval_summary;
    lv_obj_t *label_fluval_channels;
    lv_obj_t *label_wifi;
    lv_obj_t *btn_fluval_auto;
    lv_obj_t *btn_fluval_manual;
    lv_obj_t *btn_fluval_setall;
    lv_obj_t *btn_co2_on;
    lv_obj_t *btn_co2_off;
    lv_obj_t *btn_co2_auto;
    lv_obj_t *btn_options;
} fishduino_dashboard_handles_t;

fishduino_dashboard_handles_t fishduino_screen_dashboard_build(lv_obj_t *parent);

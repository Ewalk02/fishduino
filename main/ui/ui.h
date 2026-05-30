#pragma once

#include "lvgl.h"
#include "co2/co2_schedule.h"
#include "feeder/feeder_schedule.h"
#include "storage/settings_nvs.h"

typedef struct {
    lv_obj_t *root;
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
} fishduino_ui_t;

void fishduino_ui_init(fishduino_ui_t *ui);
void fishduino_ui_update(fishduino_ui_t *ui, const fishduino_co2_t *co2,
                        const fishduino_feeder_t *feeder, const fishduino_settings_t *settings);

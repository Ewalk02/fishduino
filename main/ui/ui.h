#pragma once

#include "lvgl.h"
#include "co2/co2_schedule.h"
#include "feeder/feeder_schedule.h"
#include "screen_dashboard_cockpit.h"
#include "storage/settings_nvs.h"

typedef struct {
    lv_obj_t *root;
    fishduino_cockpit_handles_t cockpit;
} fishduino_ui_t;

void fishduino_ui_init(fishduino_ui_t *ui);
void fishduino_ui_update(fishduino_ui_t *ui, const fishduino_co2_t *co2,
                        const fishduino_feeder_t *feeder, const fishduino_settings_t *settings);

#pragma once

#include "lvgl.h"

typedef struct {
    lv_obj_t *screen;
    lv_obj_t *label_tz;
} fishduino_options_handles_t;

fishduino_options_handles_t fishduino_screen_options_build(lv_obj_t *parent);
void fishduino_screen_options_show(void);

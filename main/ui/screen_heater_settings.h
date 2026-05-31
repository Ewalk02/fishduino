#pragma once

#include "lvgl.h"

void fishduino_screen_heater_build(lv_obj_t *parent);
void fishduino_screen_heater_show(void);
void fishduino_screen_heater_hide(void);
lv_obj_t *fishduino_screen_heater_root(void);
void fishduino_screen_heater_refresh(void);

#pragma once

#include "lvgl.h"

void fishduino_screen_maintenance_build(lv_obj_t *parent);
void fishduino_screen_maintenance_show(void);
void fishduino_screen_maintenance_hide(void);
lv_obj_t *fishduino_screen_maintenance_root(void);
void fishduino_screen_maintenance_refresh(void);

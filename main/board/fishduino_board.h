#pragma once

#include <stdbool.h>

#include "esp_err.h"
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t fishduino_board_init(void);

lv_display_t *fishduino_display_start(void);
bool fishduino_display_lock(uint32_t timeout_ms);
void fishduino_display_unlock(void);
void fishduino_display_backlight_on(void);

int fishduino_display_width(void);
int fishduino_display_height(void);

bool fishduino_board_is_4in(void);
bool fishduino_board_is_7b(void);
const char *fishduino_board_name(void);

void fishduino_board_log_status(void);

#ifdef __cplusplus
}
#endif

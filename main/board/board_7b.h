#pragma once

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

lv_display_t *board_7b_display_start(void);
bool board_7b_display_lock(uint32_t timeout_ms);
void board_7b_display_unlock(void);
void board_7b_display_backlight_on(void);

#ifdef __cplusplus
}
#endif

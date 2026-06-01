#pragma once

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

lv_display_t *board_4in_display_start(void);
bool board_4in_display_lock(uint32_t timeout_ms);
void board_4in_display_unlock(void);
void board_4in_display_backlight_on(void);

#ifdef __cplusplus
}
#endif

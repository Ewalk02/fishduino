#include "board_7b.h"

#include "bsp/esp32_p4_wifi6_touch_lcd_7b.h"

lv_display_t *board_7b_display_start(void)
{
    return bsp_display_start();
}

bool board_7b_display_lock(uint32_t timeout_ms)
{
    return bsp_display_lock(timeout_ms);
}

void board_7b_display_unlock(void)
{
    bsp_display_unlock();
}

void board_7b_display_backlight_on(void)
{
    bsp_display_backlight_on();
}

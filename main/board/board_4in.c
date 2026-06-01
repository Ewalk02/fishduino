#include "board_4in.h"

#include "bsp/esp32_p4_platform.h"

lv_display_t *board_4in_display_start(void)
{
    return bsp_display_start();
}

bool board_4in_display_lock(uint32_t timeout_ms)
{
    return bsp_display_lock(timeout_ms);
}

void board_4in_display_unlock(void)
{
    bsp_display_unlock();
}

void board_4in_display_backlight_on(void)
{
    bsp_display_backlight_on();
}

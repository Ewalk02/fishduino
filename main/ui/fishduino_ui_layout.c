#include "fishduino_ui_layout.h"

#include "lvgl.h"

fishduino_ui_layout_t fishduino_ui_get_layout(void)
{
    lv_display_t *disp = lv_display_get_default();
    if (disp == NULL) {
        return FISHDUINO_UI_LAYOUT_COMPACT;
    }

    const int w = (int)lv_display_get_horizontal_resolution(disp);
    const int h = (int)lv_display_get_vertical_resolution(disp);

    if (w >= 900 && h >= 500) {
        return FISHDUINO_UI_LAYOUT_WIDE;
    }

    return FISHDUINO_UI_LAYOUT_COMPACT;
}

const char *fishduino_ui_layout_name(fishduino_ui_layout_t layout)
{
    switch (layout) {
    case FISHDUINO_UI_LAYOUT_WIDE:
        return "wide";
    case FISHDUINO_UI_LAYOUT_COMPACT:
    default:
        return "compact";
    }
}

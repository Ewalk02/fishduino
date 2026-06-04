#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    FISHDUINO_UI_LAYOUT_COMPACT,
    FISHDUINO_UI_LAYOUT_WIDE,
} fishduino_ui_layout_t;

fishduino_ui_layout_t fishduino_ui_get_layout(void);
const char *fishduino_ui_layout_name(fishduino_ui_layout_t layout);

#ifdef __cplusplus
}
#endif

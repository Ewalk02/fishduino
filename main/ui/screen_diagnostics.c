#include "screen_diagnostics.h"

#include "net/status_console.h"

static lv_obj_t *s_screen;
static lv_obj_t *s_label_body;

static void btn_back_cb(lv_event_t *e)
{
    (void)e;
    fishduino_screen_diagnostics_hide();
}

void fishduino_screen_diagnostics_build(lv_obj_t *parent)
{
    s_screen = lv_obj_create(parent);
    lv_obj_set_size(s_screen, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(s_screen, lv_color_black(), 0);
    lv_obj_add_flag(s_screen, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t *title = lv_label_create(s_screen);
    lv_label_set_text(title, "Diagnostics");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 8);

    s_label_body = lv_label_create(s_screen);
    lv_obj_set_width(s_label_body, 440);
    lv_label_set_long_mode(s_label_body, LV_LABEL_LONG_WRAP);
    lv_obj_align(s_label_body, LV_ALIGN_TOP_LEFT, 12, 36);

    lv_obj_t *btn_back = lv_btn_create(s_screen);
    lv_obj_set_size(btn_back, 100, 36);
    lv_obj_align(btn_back, LV_ALIGN_BOTTOM_MID, 0, -8);
    lv_label_set_text(lv_label_create(btn_back), "BACK");
    lv_obj_add_event_cb(btn_back, btn_back_cb, LV_EVENT_CLICKED, NULL);
}

void fishduino_screen_diagnostics_refresh(void)
{
    if (s_label_body == NULL || s_screen == NULL) {
        return;
    }
    if (lv_obj_has_flag(s_screen, LV_OBJ_FLAG_HIDDEN)) {
        return;
    }

    char buf[512];
    fishduino_status_format(buf, sizeof(buf));
    lv_label_set_text(s_label_body, buf);
}

void fishduino_screen_diagnostics_show(void)
{
    if (s_screen == NULL) {
        return;
    }
    fishduino_screen_diagnostics_refresh();
    lv_obj_clear_flag(s_screen, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_screen);
}

void fishduino_screen_diagnostics_hide(void)
{
    if (s_screen != NULL) {
        lv_obj_add_flag(s_screen, LV_OBJ_FLAG_HIDDEN);
    }
}

lv_obj_t *fishduino_screen_diagnostics_root(void)
{
    return s_screen;
}

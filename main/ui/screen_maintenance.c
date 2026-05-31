#include "screen_maintenance.h"

#include <stdio.h>

#include "maintenance/maintenance_mode.h"

static lv_obj_t *s_screen;
static lv_obj_t *s_label_info;

static void hide_self(void)
{
    if (s_screen) {
        lv_obj_add_flag(s_screen, LV_OBJ_FLAG_HIDDEN);
    }
}

static void btn_back_cb(lv_event_t *e)
{
    (void)e;
    hide_self();
}

static void start_min_cb(lv_event_t *e)
{
    uint32_t min = (uint32_t)(intptr_t)lv_event_get_user_data(e);
    fishduino_maintenance_mode_start(min);
    fishduino_screen_maintenance_refresh();
}

static void btn_end_cb(lv_event_t *e)
{
    (void)e;
    fishduino_maintenance_mode_end();
    hide_self();
}

void fishduino_screen_maintenance_build(lv_obj_t *parent)
{
    s_screen = lv_obj_create(parent);
    lv_obj_set_size(s_screen, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(s_screen, lv_color_black(), 0);
    lv_obj_add_flag(s_screen, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t *title = lv_label_create(s_screen);
    lv_label_set_text(title, "Maintenance Mode");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 8);

    s_label_info = lv_label_create(s_screen);
    lv_obj_set_width(s_label_info, 440);
    lv_label_set_long_mode(s_label_info, LV_LABEL_LONG_WRAP);
    lv_obj_align(s_label_info, LV_ALIGN_TOP_LEFT, 12, 40);

    const uint32_t mins[] = {15, 30, 60};
    for (int i = 0; i < 3; i++) {
        lv_obj_t *btn = lv_btn_create(s_screen);
        lv_obj_set_size(btn, 120, 36);
        lv_obj_align(btn, LV_ALIGN_TOP_LEFT, 12 + i * 130, 120);
        char buf[16];
        snprintf(buf, sizeof(buf), "%u min", (unsigned)mins[i]);
        lv_label_set_text(lv_label_create(btn), buf);
        lv_obj_add_event_cb(btn, start_min_cb, LV_EVENT_CLICKED, (void *)(intptr_t)mins[i]);
    }

    lv_obj_t *btn_end = lv_btn_create(s_screen);
    lv_obj_set_size(btn_end, 120, 36);
    lv_obj_align(btn_end, LV_ALIGN_TOP_LEFT, 12, 170);
    lv_label_set_text(lv_label_create(btn_end), "End Now");
    lv_obj_add_event_cb(btn_end, btn_end_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *btn_back = lv_btn_create(s_screen);
    lv_obj_set_size(btn_back, 100, 36);
    lv_obj_align(btn_back, LV_ALIGN_BOTTOM_MID, 0, -8);
    lv_label_set_text(lv_label_create(btn_back), "BACK");
    lv_obj_add_event_cb(btn_back, btn_back_cb, LV_EVENT_CLICKED, NULL);
}

void fishduino_screen_maintenance_refresh(void)
{
    if (s_label_info == NULL) {
        return;
    }
    char buf[128];
    if (fishduino_maintenance_mode_is_active()) {
        snprintf(buf, sizeof(buf), "ACTIVE\nRemaining: %lld sec\nCO2 blocked.",
                 (long long)(fishduino_maintenance_mode_remaining_ms() / 1000LL));
    } else {
        snprintf(buf, sizeof(buf), "Inactive.\nCO2 OFF during filter service.\nSelect duration to start.");
    }
    lv_label_set_text(s_label_info, buf);
}

void fishduino_screen_maintenance_show(void)
{
    if (s_screen == NULL) {
        return;
    }
    fishduino_screen_maintenance_refresh();
    lv_obj_clear_flag(s_screen, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_screen);
}

void fishduino_screen_maintenance_hide(void)
{
    hide_self();
}

lv_obj_t *fishduino_screen_maintenance_root(void)
{
    return s_screen;
}

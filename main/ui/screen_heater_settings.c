#include "screen_heater_settings.h"

#include <stdio.h>
#include <string.h>

#include "heater/heater_manager.h"

static lv_obj_t *s_screen;
static lv_obj_t *s_label_body;

static void btn_back_cb(lv_event_t *e)
{
    (void)e;
    fishduino_screen_heater_hide();
}

void fishduino_screen_heater_build(lv_obj_t *parent)
{
    s_screen = lv_obj_create(parent);
    lv_obj_set_size(s_screen, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(s_screen, lv_color_black(), 0);
    lv_obj_add_flag(s_screen, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t *title = lv_label_create(s_screen);
    lv_label_set_text(title, "Chihiros Heater");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 8);

    s_label_body = lv_label_create(s_screen);
    lv_obj_set_width(s_label_body, 440);
    lv_label_set_long_mode(s_label_body, LV_LABEL_LONG_WRAP);
    lv_obj_align(s_label_body, LV_ALIGN_TOP_LEFT, 12, 40);

    lv_obj_t *btn_back = lv_btn_create(s_screen);
    lv_obj_set_size(btn_back, 100, 36);
    lv_obj_align(btn_back, LV_ALIGN_BOTTOM_MID, 0, -8);
    lv_label_set_text(lv_label_create(btn_back), "BACK");
    lv_obj_add_event_cb(btn_back, btn_back_cb, LV_EVENT_CLICKED, NULL);
}

void fishduino_screen_heater_refresh(void)
{
    if (s_label_body == NULL || s_screen == NULL) {
        return;
    }
    if (lv_obj_has_flag(s_screen, LV_OBJ_FLAG_HIDDEN)) {
        return;
    }

    heater_status_t st;
    heater_manager_get_status(&st);

    char buf[256];
    snprintf(buf, sizeof(buf),
             "Enabled: %s\nState: %s\nTemp: %.1f F\nTarget: %.1f F\nHeating: %s\nOnline: %s  Stale: %s\n%s%s",
             st.enabled ? "yes" : "no", heater_manager_state_text(st.state), (double)st.reported_temp_f,
             (double)st.target_temp_f, st.heating ? "yes" : "no", st.online ? "yes" : "no", st.stale ? "yes" : "no",
             st.alarm != HEATER_ALARM_NONE ? heater_manager_alarm_text(st.alarm) : "",
             st.error_text[0] ? "\n" : "");
    if (st.error_text[0]) {
        size_t n = strlen(buf);
        snprintf(buf + n, sizeof(buf) - n, "%s", st.error_text);
    }
    lv_label_set_text(s_label_body, buf);
}

void fishduino_screen_heater_show(void)
{
    if (s_screen == NULL) {
        return;
    }
    fishduino_screen_heater_refresh();
    lv_obj_clear_flag(s_screen, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_screen);
}

void fishduino_screen_heater_hide(void)
{
    if (s_screen) {
        lv_obj_add_flag(s_screen, LV_OBJ_FLAG_HIDDEN);
    }
}

lv_obj_t *fishduino_screen_heater_root(void)
{
    return s_screen;
}

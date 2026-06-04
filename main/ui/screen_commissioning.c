#include "screen_commissioning.h"

#include <stdio.h>

#include "shelly/shelly_manager.h"
#include "storage/settings_runtime.h"

static lv_obj_t *s_screen;
static lv_obj_t *s_label_step;
static lv_obj_t *s_label_live;
static int s_step;

static const char *s_steps[] = {
    "1/7: CO2 test lamp ON\nUse dashboard CO2 ON or serial shelly_co2_on",
    "2/7: CO2 test lamp OFF\nUse CO2 OFF or shelly_co2_off",
    "3/7: CO2 AUTO schedule\nEnable schedule; verify ON/OFF in window",
    "4/7: Filter lamp watts\nFilter line should show watts when running",
    "5/7: FILTER IS OFF\nTurn filter plug output off; expect alarm",
    "6/7: FILTER MONITOR OFFLINE\nUnplug filter Shelly or disable Wi-Fi briefly",
    "7/7: Filter never Switch.Set\nAquaPilot only polls filter plug (read-only)",
};

static void refresh_live(void)
{
    fishduino_shelly_state_t ss;
    fishduino_settings_t st;
    if (!fishduino_shelly_manager_get_state_snapshot(&ss) || !fishduino_settings_get_snapshot(&st)) {
        lv_label_set_text(s_label_live, "Loading...");
        return;
    }

    char buf[160];
    snprintf(buf, sizeof(buf),
             "CO2: %s %.1fW | Filter: %s %.1fW | Alarm: %s",
             ss.co2_status.output ? "ON" : "off", (double)ss.co2_status.watts,
             ss.filter_status.output ? "ON" : "off", (double)ss.filter_status.watts,
             fishduino_filter_alarm_text(ss.filter_alarm));
    lv_label_set_text(s_label_live, buf);
}

static void show_step(void)
{
    if (s_step < 0) {
        s_step = 0;
    }
    if (s_step > 6) {
        s_step = 6;
    }
    lv_label_set_text(s_label_step, s_steps[s_step]);
    refresh_live();
}

static void btn_back_cb(lv_event_t *e)
{
    (void)e;
    if (s_step > 0) {
        s_step--;
        show_step();
    } else {
        fishduino_screen_commissioning_hide();
    }
}

static void btn_next_cb(lv_event_t *e)
{
    (void)e;
    if (s_step < 6) {
        s_step++;
        show_step();
    } else {
        fishduino_screen_commissioning_hide();
    }
}

static void btn_exit_cb(lv_event_t *e)
{
    (void)e;
    fishduino_screen_commissioning_hide();
}

void fishduino_screen_commissioning_build(lv_obj_t *parent)
{
    s_screen = lv_obj_create(parent);
    lv_obj_set_size(s_screen, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(s_screen, lv_color_black(), 0);
    lv_obj_add_flag(s_screen, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t *title = lv_label_create(s_screen);
    lv_label_set_text(title, "Safety Test (use lamps)");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 8);

    s_label_step = lv_label_create(s_screen);
    lv_obj_set_width(s_label_step, 440);
    lv_obj_align(s_label_step, LV_ALIGN_TOP_MID, 0, 36);

    s_label_live = lv_label_create(s_screen);
    lv_obj_set_width(s_label_live, 440);
    lv_obj_align(s_label_live, LV_ALIGN_TOP_MID, 0, 120);

    lv_obj_t *btn_prev = lv_btn_create(s_screen);
    lv_obj_set_size(btn_prev, 100, 40);
    lv_obj_align(btn_prev, LV_ALIGN_BOTTOM_LEFT, 12, -12);
    lv_label_set_text(lv_label_create(btn_prev), "BACK");
    lv_obj_add_event_cb(btn_prev, btn_back_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *btn_next = lv_btn_create(s_screen);
    lv_obj_set_size(btn_next, 100, 40);
    lv_obj_align(btn_next, LV_ALIGN_BOTTOM_RIGHT, -12, -12);
    lv_label_set_text(lv_label_create(btn_next), "NEXT");
    lv_obj_add_event_cb(btn_next, btn_next_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *btn_exit = lv_btn_create(s_screen);
    lv_obj_set_size(btn_exit, 100, 36);
    lv_obj_align(btn_exit, LV_ALIGN_BOTTOM_MID, 0, -12);
    lv_label_set_text(lv_label_create(btn_exit), "DONE");
    lv_obj_add_event_cb(btn_exit, btn_exit_cb, LV_EVENT_CLICKED, NULL);
}

void fishduino_screen_commissioning_show(void)
{
    s_step = 0;
    show_step();
    if (s_screen != NULL) {
        lv_obj_clear_flag(s_screen, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(s_screen);
    }
}

void fishduino_screen_commissioning_hide(void)
{
    if (s_screen != NULL) {
        lv_obj_add_flag(s_screen, LV_OBJ_FLAG_HIDDEN);
    }
}

lv_obj_t *fishduino_screen_commissioning_root(void)
{
    return s_screen;
}

/** Called from UI update loop while commissioning visible. */
void fishduino_screen_commissioning_tick(void)
{
    if (s_screen != NULL && !lv_obj_has_flag(s_screen, LV_OBJ_FLAG_HIDDEN)) {
        refresh_live();
    }
}

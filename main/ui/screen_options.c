#include "screen_options.h"

#include <stdio.h>

#include "screen_co2_schedule.h"
#include "screen_commissioning.h"
#include "screen_diagnostics.h"
#include "screen_shelly_settings.h"
#include "screen_wifi_settings.h"
#include "shelly/shelly_manager.h"
#include "storage/settings_nvs.h"
#include "storage/settings_runtime.h"

static lv_obj_t *s_dashboard_parent;
static lv_obj_t *s_options_screen;
static lv_obj_t *s_label_status;

static bool is_overlay(lv_obj_t *child)
{
    return child == s_options_screen || child == fishduino_screen_wifi_root() ||
           child == fishduino_screen_shelly_root() || child == fishduino_screen_co2_schedule_root() ||
           child == fishduino_screen_commissioning_root() || child == fishduino_screen_diagnostics_root();
}

static void set_dashboard_visible(bool visible)
{
    if (s_dashboard_parent == NULL) {
        return;
    }
    uint32_t n = lv_obj_get_child_cnt(s_dashboard_parent);
    for (uint32_t i = 0; i < n; i++) {
        lv_obj_t *child = lv_obj_get_child(s_dashboard_parent, i);
        if (is_overlay(child)) {
            continue;
        }
        if (visible) {
            lv_obj_clear_flag(child, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(child, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

static void hide_all_overlays(void)
{
    fishduino_screen_wifi_hide();
    fishduino_screen_shelly_hide();
    fishduino_screen_co2_schedule_hide();
    fishduino_screen_commissioning_hide();
    fishduino_screen_diagnostics_hide();
}

static void hide_options(void)
{
    hide_all_overlays();
    if (s_options_screen != NULL) {
        lv_obj_add_flag(s_options_screen, LV_OBJ_FLAG_HIDDEN);
    }
    set_dashboard_visible(true);
}

static void refresh_status_label(void)
{
    if (s_label_status == NULL) {
        return;
    }
    fishduino_settings_t st;
    if (!fishduino_settings_get_snapshot(&st)) {
        return;
    }
    char buf[128];
    snprintf(buf, sizeof(buf), "%s | CO2 %02u:%02u-%02u:%02u %s\nCO2 %s | Filter %s",
             fishduino_timezone_name(st.timezone), (unsigned)(st.co2.on_min / 60), (unsigned)(st.co2.on_min % 60),
             (unsigned)(st.co2.off_min / 60), (unsigned)(st.co2.off_min % 60), st.co2.enabled ? "on" : "off",
             st.shelly_co2.enabled ? st.shelly_co2.ip : "disabled",
             st.shelly_filter.enabled ? st.shelly_filter.ip : "disabled");
    lv_label_set_text(s_label_status, buf);
}

static void btn_back_cb(lv_event_t *e)
{
    (void)e;
    hide_options();
}

static void btn_wifi_cb(lv_event_t *e)
{
    (void)e;
    fishduino_screen_wifi_show();
}

static void btn_shelly_cb(lv_event_t *e)
{
    (void)e;
    fishduino_screen_shelly_show();
}

static void btn_co2_sched_cb(lv_event_t *e)
{
    (void)e;
    fishduino_screen_co2_schedule_show();
}

static void btn_safety_cb(lv_event_t *e)
{
    (void)e;
    fishduino_screen_commissioning_show();
}

static void btn_diagnostics_cb(lv_event_t *e)
{
    (void)e;
    fishduino_screen_diagnostics_show();
}

static void btn_filter_cal_cb(lv_event_t *e)
{
    (void)e;
    fishduino_shelly_filter_calibrate_start();
}

static void set_tz_cb(lv_event_t *e)
{
    fishduino_timezone_t tz = (fishduino_timezone_t)(intptr_t)lv_event_get_user_data(e);
    fishduino_shelly_set_timezone(tz);
    refresh_status_label();
}

static void co2_sched_on_cb(lv_event_t *e)
{
    (void)e;
    fishduino_shelly_set_co2_schedule_enabled(true);
    refresh_status_label();
}

static void co2_sched_off_cb(lv_event_t *e)
{
    (void)e;
    fishduino_shelly_set_co2_schedule_enabled(false);
    refresh_status_label();
}

fishduino_options_handles_t fishduino_screen_options_build(lv_obj_t *parent)
{
    fishduino_options_handles_t h = {0};
    s_dashboard_parent = parent;

    h.screen = lv_obj_create(parent);
    s_options_screen = h.screen;
    lv_obj_set_size(h.screen, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(h.screen, lv_color_black(), 0);
    lv_obj_add_flag(h.screen, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t *title = lv_label_create(h.screen);
    lv_label_set_text(title, "Options");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 4);

    h.label_tz = lv_label_create(h.screen);
    s_label_status = h.label_tz;
    lv_obj_set_width(h.label_tz, 440);
    lv_obj_set_style_text_align(h.label_tz, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(h.label_tz, LV_ALIGN_TOP_MID, 0, 24);
    refresh_status_label();

    lv_obj_t *sched_lbl = lv_label_create(h.screen);
    lv_label_set_text(sched_lbl, "CO2 schedule enable:");
    lv_obj_align(sched_lbl, LV_ALIGN_TOP_LEFT, 8, 72);

    lv_obj_t *btn_sched_on = lv_btn_create(h.screen);
    lv_obj_set_size(btn_sched_on, 88, 28);
    lv_obj_align(btn_sched_on, LV_ALIGN_TOP_LEFT, 8, 92);
    lv_label_set_text(lv_label_create(btn_sched_on), "ON");
    lv_obj_add_event_cb(btn_sched_on, co2_sched_on_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *btn_sched_off = lv_btn_create(h.screen);
    lv_obj_set_size(btn_sched_off, 88, 28);
    lv_obj_align(btn_sched_off, LV_ALIGN_TOP_LEFT, 100, 92);
    lv_label_set_text(lv_label_create(btn_sched_off), "OFF");
    lv_obj_add_event_cb(btn_sched_off, co2_sched_off_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *btn_co2_times = lv_btn_create(h.screen);
    lv_obj_set_size(btn_co2_times, 200, 32);
    lv_obj_align(btn_co2_times, LV_ALIGN_TOP_LEFT, 200, 88);
    lv_label_set_text(lv_label_create(btn_co2_times), "CO2 Schedule");
    lv_obj_add_event_cb(btn_co2_times, btn_co2_sched_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *btn_wifi = lv_btn_create(h.screen);
    lv_obj_set_size(btn_wifi, 200, 32);
    lv_obj_align(btn_wifi, LV_ALIGN_TOP_LEFT, 8, 128);
    lv_label_set_text(lv_label_create(btn_wifi), "Wi-Fi Settings");
    lv_obj_add_event_cb(btn_wifi, btn_wifi_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *btn_shelly = lv_btn_create(h.screen);
    lv_obj_set_size(btn_shelly, 200, 32);
    lv_obj_align(btn_shelly, LV_ALIGN_TOP_LEFT, 220, 128);
    lv_label_set_text(lv_label_create(btn_shelly), "Shelly Settings");
    lv_obj_add_event_cb(btn_shelly, btn_shelly_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *btn_safety = lv_btn_create(h.screen);
    lv_obj_set_size(btn_safety, 200, 32);
    lv_obj_align(btn_safety, LV_ALIGN_TOP_LEFT, 8, 168);
    lv_label_set_text(lv_label_create(btn_safety), "Safety Test");
    lv_obj_add_event_cb(btn_safety, btn_safety_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *btn_cal = lv_btn_create(h.screen);
    lv_obj_set_size(btn_cal, 200, 32);
    lv_obj_align(btn_cal, LV_ALIGN_TOP_LEFT, 220, 168);
    lv_label_set_text(lv_label_create(btn_cal), "Filter Calibrate");
    lv_obj_add_event_cb(btn_cal, btn_filter_cal_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *btn_diag = lv_btn_create(h.screen);
    lv_obj_set_size(btn_diag, 200, 32);
    lv_obj_align(btn_diag, LV_ALIGN_TOP_LEFT, 8, 208);
    lv_label_set_text(lv_label_create(btn_diag), "Diagnostics");
    lv_obj_add_event_cb(btn_diag, btn_diagnostics_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *tz_lbl = lv_label_create(h.screen);
    lv_label_set_text(tz_lbl, "Timezone:");
    lv_obj_align(tz_lbl, LV_ALIGN_TOP_LEFT, 8, 248);

    const struct {
        const char *name;
        fishduino_timezone_t tz;
    } zones[] = {
        {"East", FISHDUINO_TZ_US_EASTERN},
        {"Cent", FISHDUINO_TZ_US_CENTRAL},
        {"Mtn", FISHDUINO_TZ_US_MOUNTAIN},
        {"Pac", FISHDUINO_TZ_US_PACIFIC},
    };

    for (int i = 0; i < 4; i++) {
        lv_obj_t *btn = lv_btn_create(h.screen);
        lv_obj_set_size(btn, 100, 28);
        lv_obj_align(btn, LV_ALIGN_TOP_LEFT, 8 + (i % 2) * 108, 268 + (i / 2) * 32);
        lv_label_set_text(lv_label_create(btn), zones[i].name);
        lv_obj_add_event_cb(btn, set_tz_cb, LV_EVENT_CLICKED, (void *)(intptr_t)zones[i].tz);
    }

    lv_obj_t *btn_back = lv_btn_create(h.screen);
    lv_obj_set_size(btn_back, 100, 36);
    lv_obj_align(btn_back, LV_ALIGN_BOTTOM_MID, 0, -4);
    lv_label_set_text(lv_label_create(btn_back), "BACK");
    lv_obj_add_event_cb(btn_back, btn_back_cb, LV_EVENT_CLICKED, NULL);

    fishduino_screen_wifi_build(parent);
    fishduino_screen_shelly_build(parent);
    fishduino_screen_co2_schedule_build(parent);
    fishduino_screen_commissioning_build(parent);
    fishduino_screen_diagnostics_build(parent);

    return h;
}

void fishduino_screen_options_show(void)
{
    if (s_options_screen != NULL) {
        refresh_status_label();
        lv_obj_clear_flag(s_options_screen, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(s_options_screen);
    }
    set_dashboard_visible(false);
}

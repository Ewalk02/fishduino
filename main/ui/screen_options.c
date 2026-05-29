#include "screen_options.h"

#include <stdio.h>

#include "screen_wifi_settings.h"
#include "shelly/shelly_manager.h"
#include "storage/settings_nvs.h"

static lv_obj_t *s_dashboard_parent;
static lv_obj_t *s_options_screen;
static lv_obj_t *s_label_status;

static void set_dashboard_visible(bool visible)
{
    if (s_dashboard_parent == NULL) {
        return;
    }
    uint32_t n = lv_obj_get_child_cnt(s_dashboard_parent);
    for (uint32_t i = 0; i < n; i++) {
        lv_obj_t *child = lv_obj_get_child(s_dashboard_parent, i);
        if (child == s_options_screen || child == fishduino_screen_wifi_root()) {
            continue;
        }
        if (visible) {
            lv_obj_clear_flag(child, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(child, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

static void hide_options(void)
{
    fishduino_screen_wifi_hide();
    if (s_options_screen != NULL) {
        lv_obj_add_flag(s_options_screen, LV_OBJ_FLAG_HIDDEN);
    }
    set_dashboard_visible(true);
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

static void set_tz_cb(lv_event_t *e)
{
    fishduino_timezone_t tz = (fishduino_timezone_t)(intptr_t)lv_event_get_user_data(e);
    fishduino_shelly_set_timezone(tz);
    fishduino_settings_t *st = fishduino_shelly_manager_get_settings_mutable();
    if (s_label_status != NULL) {
        char buf[72];
        snprintf(buf, sizeof(buf), "Timezone: %s (CO2 sched %s)", fishduino_timezone_name(st->timezone),
                 st->co2.enabled ? "on" : "off");
        lv_label_set_text(s_label_status, buf);
    }
}

static void co2_sched_on_cb(lv_event_t *e)
{
    (void)e;
    fishduino_shelly_set_co2_schedule_enabled(true);
}

static void co2_sched_off_cb(lv_event_t *e)
{
    (void)e;
    fishduino_shelly_set_co2_schedule_enabled(false);
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
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 8);

    h.label_tz = lv_label_create(h.screen);
    s_label_status = h.label_tz;
    fishduino_settings_t *st = fishduino_shelly_manager_get_settings_mutable();
    char buf[72];
    snprintf(buf, sizeof(buf), "Timezone: %s", fishduino_timezone_name(st->timezone));
    lv_label_set_text(h.label_tz, buf);
    lv_obj_align(h.label_tz, LV_ALIGN_TOP_MID, 0, 36);

    lv_obj_t *sched_lbl = lv_label_create(h.screen);
    lv_label_set_text(sched_lbl, "CO2 schedule:");
    lv_obj_align(sched_lbl, LV_ALIGN_TOP_LEFT, 12, 64);

    lv_obj_t *btn_sched_on = lv_btn_create(h.screen);
    lv_obj_set_size(btn_sched_on, 100, 32);
    lv_obj_align(btn_sched_on, LV_ALIGN_TOP_LEFT, 12, 86);
    lv_obj_t *lon = lv_label_create(btn_sched_on);
    lv_label_set_text(lon, "ON");
    lv_obj_center(lon);
    lv_obj_add_event_cb(btn_sched_on, co2_sched_on_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *btn_sched_off = lv_btn_create(h.screen);
    lv_obj_set_size(btn_sched_off, 100, 32);
    lv_obj_align(btn_sched_off, LV_ALIGN_TOP_LEFT, 120, 86);
    lv_obj_t *loff = lv_label_create(btn_sched_off);
    lv_label_set_text(loff, "OFF");
    lv_obj_center(loff);
    lv_obj_add_event_cb(btn_sched_off, co2_sched_off_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *btn_wifi = lv_btn_create(h.screen);
    lv_obj_set_size(btn_wifi, 208, 36);
    lv_obj_align(btn_wifi, LV_ALIGN_TOP_LEFT, 12, 128);
    lv_obj_t *lwifi = lv_label_create(btn_wifi);
    lv_label_set_text(lwifi, "Wi-Fi Settings");
    lv_obj_center(lwifi);
    lv_obj_add_event_cb(btn_wifi, btn_wifi_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *tz_lbl = lv_label_create(h.screen);
    lv_label_set_text(tz_lbl, "Timezone:");
    lv_obj_align(tz_lbl, LV_ALIGN_TOP_LEFT, 12, 176);

    const struct {
        const char *name;
        fishduino_timezone_t tz;
    } zones[] = {
        {"Eastern", FISHDUINO_TZ_US_EASTERN},
        {"Central", FISHDUINO_TZ_US_CENTRAL},
        {"Mountain", FISHDUINO_TZ_US_MOUNTAIN},
        {"Pacific", FISHDUINO_TZ_US_PACIFIC},
    };

    for (int i = 0; i < 4; i++) {
        lv_obj_t *btn = lv_btn_create(h.screen);
        lv_obj_set_size(btn, 100, 32);
        lv_obj_align(btn, LV_ALIGN_TOP_LEFT, 12 + (i % 2) * 108, 198 + (i / 2) * 36);
        lv_obj_t *lbl = lv_label_create(btn);
        lv_label_set_text(lbl, zones[i].name);
        lv_obj_center(lbl);
        lv_obj_add_event_cb(btn, set_tz_cb, LV_EVENT_CLICKED, (void *)(intptr_t)zones[i].tz);
    }

    lv_obj_t *btn_back = lv_btn_create(h.screen);
    lv_obj_set_size(btn_back, 120, 40);
    lv_obj_align(btn_back, LV_ALIGN_BOTTOM_MID, 0, -8);
    lv_obj_t *lback = lv_label_create(btn_back);
    lv_label_set_text(lback, "BACK");
    lv_obj_center(lback);
    lv_obj_add_event_cb(btn_back, btn_back_cb, LV_EVENT_CLICKED, NULL);

    fishduino_screen_wifi_build(parent);

    return h;
}

void fishduino_screen_options_show(void)
{
    if (s_options_screen != NULL) {
        fishduino_settings_t *st = fishduino_shelly_manager_get_settings_mutable();
        char buf[72];
        snprintf(buf, sizeof(buf), "Timezone: %s (CO2 sched %s)", fishduino_timezone_name(st->timezone),
                 st->co2.enabled ? "on" : "off");
        if (s_label_status != NULL) {
            lv_label_set_text(s_label_status, buf);
        }
        lv_obj_clear_flag(s_options_screen, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(s_options_screen);
    }
    set_dashboard_visible(false);
}

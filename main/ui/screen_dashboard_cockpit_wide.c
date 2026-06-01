#include "screen_dashboard_cockpit.h"

#include <stdio.h>
#include <string.h>

#include "screen_co2_schedule.h"
#include "screen_fluval_settings.h"
#include "screen_heater_settings.h"
#include "screen_maint_tracker.h"
#include "screen_options.h"
#include "screen_water_tests.h"

#define GAUGE_START 135
#define GAUGE_END   405
#define NAV_COUNT   6

static void tap_heater_cb(lv_event_t *e)
{
    (void)e;
    fishduino_screen_heater_show();
}

static void tap_filter_cb(lv_event_t *e)
{
    (void)e;
    fishduino_screen_options_show();
}

static void tap_co2_cb(lv_event_t *e)
{
    (void)e;
    fishduino_screen_co2_schedule_show();
}

static void tap_feeder_cb(lv_event_t *e)
{
    (void)e;
    fishduino_screen_options_show();
}

static void tap_water_cb(lv_event_t *e)
{
    (void)e;
    fishduino_screen_water_tests_show();
}

static void tap_reminders_cb(lv_event_t *e)
{
    (void)e;
    fishduino_screen_maint_tracker_show();
}

static void tap_light_cb(lv_event_t *e)
{
    (void)e;
    fishduino_screen_fluval_show();
}

static void tap_options_cb(lv_event_t *e)
{
    (void)e;
    fishduino_screen_options_show();
}

static lv_obj_t *make_panel(lv_obj_t *parent, lv_coord_t x, lv_coord_t y, lv_coord_t w, lv_coord_t h)
{
    lv_obj_t *p = lv_obj_create(parent);
    cockpit_style_apply_panel(p);
    lv_obj_set_size(p, w, h);
    lv_obj_set_pos(p, x, y);
    lv_obj_remove_flag(p, LV_OBJ_FLAG_SCROLLABLE);
    return p;
}

static lv_obj_t *panel_title(lv_obj_t *panel, const char *text)
{
    lv_obj_t *lbl = lv_label_create(panel);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_color(lbl, cockpit_color_cyan(), 0);
    lv_obj_set_style_text_font(lbl, cockpit_font_title(), 0);
    lv_obj_align(lbl, LV_ALIGN_TOP_LEFT, 0, 0);
    return lbl;
}

static lv_obj_t *make_text_btn(lv_obj_t *parent, const char *text, lv_event_cb_t cb)
{
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x1a2430), 0);
    lv_obj_set_style_border_color(btn, lv_color_hex(0x445566), 0);
    lv_obj_set_style_border_width(btn, 1, 0);
    lv_obj_set_style_radius(btn, 4, 0);
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_font(lbl, cockpit_font_small(), 0);
    lv_obj_center(lbl);
    return btn;
}

static lv_obj_t *make_badge(lv_obj_t *parent, lv_coord_t x, lv_coord_t y)
{
    lv_obj_t *b = lv_obj_create(parent);
    lv_obj_remove_style_all(b);
    lv_obj_set_size(b, 36, 16);
    lv_obj_set_pos(b, x, y);
    lv_obj_set_style_bg_color(b, lv_color_hex(0x1a2838), 0);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(b, 4, 0);
    return b;
}

static void make_water_column(lv_obj_t *panel, lv_coord_t x, const char *name, lv_obj_t **val_lbl,
                              lv_obj_t **badge)
{
    lv_obj_t *nm = lv_label_create(panel);
    lv_label_set_text(nm, name);
    lv_obj_set_style_text_color(nm, cockpit_color_dim(), 0);
    lv_obj_set_style_text_font(nm, cockpit_font_small(), 0);
    lv_obj_set_pos(nm, x, 18);
    *val_lbl = lv_label_create(panel);
    lv_label_set_text(*val_lbl, "--");
    lv_obj_set_style_text_color(*val_lbl, cockpit_color_text(), 0);
    lv_obj_set_style_text_font(*val_lbl, cockpit_font_body(), 0);
    lv_obj_set_pos(*val_lbl, x, 34);
    *badge = make_badge(panel, x, 58);
}

void fishduino_cockpit_dashboard_build_wide(lv_obj_t *parent, fishduino_cockpit_handles_t *out)
{
    memset(out, 0, sizeof(*out));

    lv_obj_set_style_bg_color(parent, lv_color_hex(0x0a0e14), 0);
    lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, 0);

    out->banner = make_panel(parent, 8, 4, 1008, 28);
    lv_obj_set_style_bg_color(out->banner, lv_color_hex(0x331111), 0);
    lv_obj_add_flag(out->banner, LV_OBJ_FLAG_HIDDEN);
    out->lbl_banner = lv_label_create(out->banner);
    lv_label_set_text(out->lbl_banner, "");
    lv_obj_set_style_text_color(out->lbl_banner, cockpit_color_red(), 0);
    lv_obj_set_style_text_font(out->lbl_banner, cockpit_font_body(), 0);
    lv_obj_center(out->lbl_banner);

    out->lbl_brand = lv_label_create(parent);
    lv_label_set_text(out->lbl_brand, "FISHDUINO");
    lv_obj_set_style_text_color(out->lbl_brand, cockpit_color_cyan(), 0);
    lv_obj_set_style_text_font(out->lbl_brand, cockpit_font_title(), 0);
    lv_obj_align(out->lbl_brand, LV_ALIGN_TOP_MID, 0, 28);

    cockpit_gauge_create(&out->gauge_filter, parent, 8, 48, 170, "FILTER", "WATTS",
                         GAUGE_START, GAUGE_END, 0.0f, 30.0f);
    cockpit_gauge_set_scale_ticks(&out->gauge_filter, "0", "30W");
    lv_obj_add_flag(out->gauge_filter.root, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(out->gauge_filter.root, tap_filter_cb, LV_EVENT_CLICKED, NULL);

    cockpit_gauge_create(&out->gauge_temp, parent, 196, 44, 200, "TEMP", "TANK F",
                         GAUGE_START, GAUGE_END, 70.0f, 90.0f);
    cockpit_gauge_set_scale_ticks(&out->gauge_temp, "70", "90");
    lv_obj_add_flag(out->gauge_temp.root, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(out->gauge_temp.root, tap_heater_cb, LV_EVENT_CLICKED, NULL);

    out->lbl_temp_setpoint = lv_label_create(out->gauge_temp.ring_outer);
    lv_label_set_text(out->lbl_temp_setpoint, "SET --");
    lv_obj_set_style_text_color(out->lbl_temp_setpoint, cockpit_color_dim(), 0);
    lv_obj_set_style_text_font(out->lbl_temp_setpoint, cockpit_font_small(), 0);
    lv_obj_align(out->lbl_temp_setpoint, LV_ALIGN_CENTER, 0, 42);

    out->lbl_heater_relay = lv_label_create(out->gauge_temp.root);
    lv_label_set_text(out->lbl_heater_relay, "RELAY --");
    lv_obj_set_style_text_color(out->lbl_heater_relay, cockpit_color_amber(), 0);
    lv_obj_set_style_text_font(out->lbl_heater_relay, cockpit_font_small(), 0);
    lv_obj_align(out->lbl_heater_relay, LV_ALIGN_BOTTOM_MID, 0, -36);

    out->lbl_heater_shelly = lv_label_create(out->gauge_temp.root);
    lv_label_set_text(out->lbl_heater_shelly, "SHELLY --");
    lv_obj_set_style_text_color(out->lbl_heater_shelly, cockpit_color_dim(), 0);
    lv_obj_set_style_text_font(out->lbl_heater_shelly, cockpit_font_small(), 0);
    lv_obj_align(out->lbl_heater_shelly, LV_ALIGN_BOTTOM_MID, 0, -52);

    cockpit_gauge_create(&out->gauge_co2, parent, 412, 48, 170, "CO2", "INJECTION",
                         GAUGE_START, GAUGE_END, 0.0f, 1.0f);
    cockpit_gauge_set_scale_ticks(&out->gauge_co2, "OFF", "ON");
    lv_obj_add_flag(out->gauge_co2.root, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(out->gauge_co2.root, tap_co2_cb, LV_EVENT_CLICKED, NULL);

    cockpit_gauge_create(&out->gauge_feeder, parent, 846, 48, 170, "FEEDER", "SCHEDULE",
                         GAUGE_START, GAUGE_END, 0.0f, 1.0f);
    lv_obj_add_flag(out->gauge_feeder.root, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(out->gauge_feeder.root, tap_feeder_cb, LV_EVENT_CLICKED, NULL);

    out->panel_water = make_panel(parent, 8, 198, 260, 168);
    panel_title(out->panel_water, "WATER");
    lv_obj_add_flag(out->panel_water, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(out->panel_water, tap_water_cb, LV_EVENT_CLICKED, NULL);
    make_water_column(out->panel_water, 8, "pH", &out->lbl_water_ph, &out->badge_water_ph);
    make_water_column(out->panel_water, 68, "NH3", &out->lbl_water_nh3, &out->badge_water_nh3);
    make_water_column(out->panel_water, 128, "NO2", &out->lbl_water_no2, &out->badge_water_no2);
    make_water_column(out->panel_water, 188, "NO3", &out->lbl_water_no3, &out->badge_water_no3);
    out->lbl_water_updated = lv_label_create(out->panel_water);
    lv_label_set_text(out->lbl_water_updated, "Updated --");
    lv_obj_set_style_text_color(out->lbl_water_updated, cockpit_color_dim(), 0);
    lv_obj_set_style_text_font(out->lbl_water_updated, cockpit_font_small(), 0);
    lv_obj_align(out->lbl_water_updated, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    out->btn_water_view = make_text_btn(out->panel_water, "VIEW ALL", tap_water_cb);
    lv_obj_set_size(out->btn_water_view, 80, 24);
    lv_obj_align(out->btn_water_view, LV_ALIGN_BOTTOM_RIGHT, 0, 0);

    out->panel_reminders = make_panel(parent, 8, 374, 260, 118);
    panel_title(out->panel_reminders, "REMINDERS");
    lv_obj_add_flag(out->panel_reminders, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(out->panel_reminders, tap_reminders_cb, LV_EVENT_CLICKED, NULL);
    out->lbl_reminders_due = lv_label_create(out->panel_reminders);
    lv_label_set_text(out->lbl_reminders_due, "0 DUE");
    lv_obj_set_style_text_color(out->lbl_reminders_due, cockpit_color_text(), 0);
    lv_obj_set_style_text_font(out->lbl_reminders_due, cockpit_font_value(), 0);
    lv_obj_align(out->lbl_reminders_due, LV_ALIGN_LEFT_MID, 0, -8);
    out->lbl_reminders_next = lv_label_create(out->panel_reminders);
    lv_label_set_text(out->lbl_reminders_next, "NEXT: --");
    lv_obj_set_style_text_color(out->lbl_reminders_next, cockpit_color_dim(), 0);
    lv_obj_set_style_text_font(out->lbl_reminders_next, cockpit_font_small(), 0);
    lv_obj_align(out->lbl_reminders_next, LV_ALIGN_BOTTOM_LEFT, 0, 8);
    out->btn_reminders_view = make_text_btn(out->panel_reminders, "VIEW ALL", tap_reminders_cb);
    lv_obj_set_size(out->btn_reminders_view, 80, 24);
    lv_obj_align(out->btn_reminders_view, LV_ALIGN_BOTTOM_RIGHT, 0, 0);

    out->panel_trend = make_panel(parent, 276, 198, 464, 294);
    panel_title(out->panel_trend, "TEMP TREND 24H");
    out->lbl_trend_min = lv_label_create(out->panel_trend);
    lv_label_set_text(out->lbl_trend_min, "MIN --");
    lv_obj_set_style_text_color(out->lbl_trend_min, cockpit_color_dim(), 0);
    lv_obj_set_style_text_font(out->lbl_trend_min, cockpit_font_small(), 0);
    lv_obj_align(out->lbl_trend_min, LV_ALIGN_TOP_RIGHT, 0, 0);
    out->lbl_trend_max = lv_label_create(out->panel_trend);
    lv_label_set_text(out->lbl_trend_max, "MAX --");
    lv_obj_set_style_text_color(out->lbl_trend_max, cockpit_color_dim(), 0);
    lv_obj_set_style_text_font(out->lbl_trend_max, cockpit_font_small(), 0);
    lv_obj_align(out->lbl_trend_max, LV_ALIGN_TOP_RIGHT, 0, 14);

    out->chart_temp = lv_chart_create(out->panel_trend);
    lv_obj_set_size(out->chart_temp, 448, 248);
    lv_obj_align(out->chart_temp, LV_ALIGN_BOTTOM_MID, 0, -4);
    lv_obj_set_style_bg_color(out->chart_temp, lv_color_hex(0x0c1218), 0);
    lv_obj_set_style_border_color(out->chart_temp, lv_color_hex(0x334455), 0);
    lv_obj_set_style_border_width(out->chart_temp, 1, 0);
    lv_chart_set_type(out->chart_temp, LV_CHART_TYPE_LINE);
    lv_chart_set_range(out->chart_temp, LV_CHART_AXIS_PRIMARY_Y, 0, 100);
    lv_chart_set_point_count(out->chart_temp, DASHBOARD_TEMP_HISTORY_LEN);
    out->chart_temp_series = lv_chart_add_series(out->chart_temp, cockpit_color_cyan(),
                                                 LV_CHART_AXIS_PRIMARY_Y);

    out->panel_clock = make_panel(parent, 748, 198, 268, 108);
    panel_title(out->panel_clock, "CLOCK");
    out->lbl_clock_time = lv_label_create(out->panel_clock);
    lv_label_set_text(out->lbl_clock_time, "--:--");
    lv_obj_set_style_text_color(out->lbl_clock_time, cockpit_color_text(), 0);
    lv_obj_set_style_text_font(out->lbl_clock_time, cockpit_font_value(), 0);
    lv_obj_align(out->lbl_clock_time, LV_ALIGN_LEFT_MID, 0, -10);
    out->lbl_clock_date = lv_label_create(out->panel_clock);
    lv_label_set_text(out->lbl_clock_date, "---");
    lv_obj_set_style_text_color(out->lbl_clock_date, cockpit_color_dim(), 0);
    lv_obj_set_style_text_font(out->lbl_clock_date, cockpit_font_small(), 0);
    lv_obj_align(out->lbl_clock_date, LV_ALIGN_BOTTOM_LEFT, 0, 8);
    out->badge_mode = lv_obj_create(out->panel_clock);
    lv_obj_remove_style_all(out->badge_mode);
    lv_obj_set_size(out->badge_mode, 72, 24);
    lv_obj_set_style_bg_color(out->badge_mode, lv_color_hex(0x0d3320), 0);
    lv_obj_set_style_bg_opa(out->badge_mode, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(out->badge_mode, 6, 0);
    lv_obj_align(out->badge_mode, LV_ALIGN_TOP_RIGHT, 0, 16);
    lv_obj_t *mode_lbl = lv_label_create(out->badge_mode);
    lv_label_set_text(mode_lbl, "AUTO");
    lv_obj_set_style_text_color(mode_lbl, cockpit_color_green(), 0);
    lv_obj_set_style_text_font(mode_lbl, cockpit_font_small(), 0);
    lv_obj_center(mode_lbl);

    out->panel_light = make_panel(parent, 748, 314, 268, 88);
    panel_title(out->panel_light, "LIGHTING");
    lv_obj_add_flag(out->panel_light, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(out->panel_light, tap_light_cb, LV_EVENT_CLICKED, NULL);
    out->lbl_light_mode = lv_label_create(out->panel_light);
    lv_label_set_text(out->lbl_light_mode, "DAYLIGHT");
    lv_obj_set_style_text_color(out->lbl_light_mode, cockpit_color_text(), 0);
    lv_obj_set_style_text_font(out->lbl_light_mode, cockpit_font_body(), 0);
    lv_obj_align(out->lbl_light_mode, LV_ALIGN_TOP_LEFT, 0, 18);
    out->bar_light = lv_bar_create(out->panel_light);
    lv_obj_set_size(out->bar_light, 240, 12);
    lv_obj_align(out->bar_light, LV_ALIGN_LEFT_MID, 0, 8);
    lv_bar_set_range(out->bar_light, 0, 100);
    lv_obj_set_style_bg_color(out->bar_light, lv_color_hex(0x1a2838), LV_PART_MAIN);
    lv_obj_set_style_bg_color(out->bar_light, cockpit_color_cyan(), LV_PART_INDICATOR);
    out->lbl_light_sunrise = lv_label_create(out->panel_light);
    lv_label_set_text(out->lbl_light_sunrise, "SUN --:--");
    lv_obj_set_style_text_color(out->lbl_light_sunrise, cockpit_color_dim(), 0);
    lv_obj_set_style_text_font(out->lbl_light_sunrise, cockpit_font_small(), 0);
    lv_obj_align(out->lbl_light_sunrise, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    out->lbl_light_sunset = lv_label_create(out->panel_light);
    lv_label_set_text(out->lbl_light_sunset, "SET --:--");
    lv_obj_set_style_text_color(out->lbl_light_sunset, cockpit_color_dim(), 0);
    lv_obj_set_style_text_font(out->lbl_light_sunset, cockpit_font_small(), 0);
    lv_obj_align(out->lbl_light_sunset, LV_ALIGN_BOTTOM_RIGHT, 0, 0);

    out->panel_systems = make_panel(parent, 748, 408, 268, 184);
    panel_title(out->panel_systems, "SYSTEMS");
    struct {
        const char *name;
        lv_coord_t x;
        lv_coord_t y;
        lv_obj_t **led;
    } sys_leds[] = {
        {"Wi-Fi", 0, 20, &out->led_wifi},
        {"Shelly CO2", 0, 42, &out->led_co2},
        {"Filter", 0, 64, &out->led_filter},
        {"Heater", 0, 86, &out->led_heater},
        {"BLE", 130, 20, &out->led_ble},
        {"Feeder", 130, 42, &out->led_feeder},
        {"Light", 130, 64, &out->led_light},
        {"Alerts", 130, 86, &out->led_alerts},
    };
    for (size_t i = 0; i < sizeof(sys_leds) / sizeof(sys_leds[0]); i++) {
        lv_obj_t *lbl = lv_label_create(out->panel_systems);
        lv_label_set_text(lbl, sys_leds[i].name);
        lv_obj_set_style_text_color(lbl, cockpit_color_dim(), 0);
        lv_obj_set_style_text_font(lbl, cockpit_font_small(), 0);
        lv_obj_set_pos(lbl, sys_leds[i].x, sys_leds[i].y);
        lv_obj_t *led = lv_led_create(out->panel_systems);
        lv_obj_set_size(led, 12, 12);
        lv_obj_set_pos(led, sys_leds[i].x + 100, sys_leds[i].y + 2);
        lv_led_off(led);
        *sys_leds[i].led = led;
    }

    out->nav_bar = lv_obj_create(parent);
    cockpit_style_apply_nav_bar(out->nav_bar);
    lv_obj_set_size(out->nav_bar, 1008, 52);
    lv_obj_set_pos(out->nav_bar, 8, 536);
    lv_obj_set_flex_flow(out->nav_bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(out->nav_bar, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_remove_flag(out->nav_bar, LV_OBJ_FLAG_SCROLLABLE);

    static const struct {
        const char *label;
        lv_event_cb_t cb;
    } nav_items[NAV_COUNT] = {
        {"Dash", NULL},
        {"Light", tap_light_cb},
        {"Feed", tap_feeder_cb},
        {"CO2", tap_co2_cb},
        {"Maint", tap_reminders_cb},
        {"Set", tap_options_cb},
    };
    for (int i = 0; i < NAV_COUNT; i++) {
        out->nav_btns[i] = lv_btn_create(out->nav_bar);
        lv_obj_set_size(out->nav_btns[i], 150, 40);
        lv_obj_set_style_bg_color(out->nav_btns[i], lv_color_hex(0x141c24), 0);
        lv_obj_set_style_border_color(out->nav_btns[i], lv_color_hex(0x334455), 0);
        lv_obj_set_style_border_width(out->nav_btns[i], 1, 0);
        if (nav_items[i].cb) {
            lv_obj_add_event_cb(out->nav_btns[i], nav_items[i].cb, LV_EVENT_CLICKED, NULL);
        }
        lv_obj_t *lbl = lv_label_create(out->nav_btns[i]);
        lv_label_set_text(lbl, nav_items[i].label);
        lv_obj_set_style_text_font(lbl, cockpit_font_small(), 0);
        lv_obj_set_style_text_color(lbl, cockpit_color_cyan(), 0);
        lv_obj_center(lbl);
    }
}

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

#define NAV_COUNT 6

static fishduino_cockpit_handles_t s_handles;

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
    lv_obj_set_style_radius(b, 4, 0);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
    lv_obj_t *lbl = lv_label_create(b);
    lv_label_set_text(lbl, "OK");
    lv_obj_set_style_text_font(lbl, cockpit_font_small(), 0);
    lv_obj_center(lbl);
    return b;
}

static void style_badge(lv_obj_t *badge, bool ok, bool unknown)
{
    if (badge == NULL) {
        return;
    }
    lv_obj_t *lbl = lv_obj_get_child(badge, 0);
    if (unknown) {
        lv_obj_set_style_bg_color(badge, lv_color_hex(0x2a3848), 0);
        lv_obj_set_style_text_color(lbl, cockpit_color_dim(), 0);
        lv_label_set_text(lbl, "--");
    } else if (ok) {
        lv_obj_set_style_bg_color(badge, lv_color_hex(0x0d3320), 0);
        lv_obj_set_style_text_color(lbl, cockpit_color_green(), 0);
        lv_label_set_text(lbl, "OK");
    } else {
        lv_obj_set_style_bg_color(badge, lv_color_hex(0x3d1818), 0);
        lv_obj_set_style_text_color(lbl, cockpit_color_red(), 0);
        lv_label_set_text(lbl, "!");
    }
}

static void apply_led(lv_obj_t *led, dashboard_led_state_t st)
{
    if (led == NULL) {
        return;
    }
    switch (st) {
    case DASHBOARD_LED_OK:
        lv_led_set_color(led, cockpit_color_green());
        lv_led_on(led);
        break;
    case DASHBOARD_LED_WARN:
        lv_led_set_color(led, cockpit_color_amber());
        lv_led_on(led);
        break;
    case DASHBOARD_LED_FAULT:
        lv_led_set_color(led, cockpit_color_red());
        lv_led_on(led);
        break;
    case DASHBOARD_LED_NA:
        lv_led_set_color(led, lv_color_hex(0x556677));
        lv_led_off(led);
        break;
    default:
        lv_led_off(led);
        break;
    }
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

fishduino_cockpit_handles_t fishduino_cockpit_dashboard_build(lv_obj_t *parent)
{
    fishduino_cockpit_handles_t h = {0};

    lv_obj_set_style_bg_color(parent, lv_color_hex(0x0a0e14), 0);
    lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, 0);

    h.banner = make_panel(parent, 8, 4, 464, 28);
    lv_obj_set_style_bg_color(h.banner, lv_color_hex(0x331111), 0);
    lv_obj_add_flag(h.banner, LV_OBJ_FLAG_HIDDEN);
    h.lbl_banner = lv_label_create(h.banner);
    lv_label_set_text(h.lbl_banner, "");
    lv_obj_set_style_text_color(h.lbl_banner, cockpit_color_red(), 0);
    lv_obj_set_style_text_font(h.lbl_banner, cockpit_font_body(), 0);
    lv_obj_center(h.lbl_banner);

    h.lbl_brand = lv_label_create(parent);
    lv_label_set_text(h.lbl_brand, "FISHDUINO");
    lv_obj_set_style_text_color(h.lbl_brand, cockpit_color_cyan(), 0);
    lv_obj_set_style_text_font(h.lbl_brand, cockpit_font_title(), 0);
    lv_obj_align(h.lbl_brand, LV_ALIGN_TOP_MID, 0, 34);

    cockpit_gauge_create(&h.gauge_filter, parent, 6, 52, 118, "FILTER", "WATTS",
                         GAUGE_START, GAUGE_END, 0.0f, 30.0f);
    cockpit_gauge_set_scale_ticks(&h.gauge_filter, "0", "30W");
    lv_obj_add_flag(h.gauge_filter.root, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(h.gauge_filter.root, tap_filter_cb, LV_EVENT_CLICKED, NULL);

    cockpit_gauge_create(&h.gauge_temp, parent, 128, 44, 224, "TEMP", "TANK F",
                         GAUGE_START, GAUGE_END, 70.0f, 90.0f);
    cockpit_gauge_set_scale_ticks(&h.gauge_temp, "70", "90");
    lv_obj_add_flag(h.gauge_temp.root, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(h.gauge_temp.root, tap_heater_cb, LV_EVENT_CLICKED, NULL);

    h.lbl_temp_setpoint = lv_label_create(h.gauge_temp.ring_outer);
    lv_label_set_text(h.lbl_temp_setpoint, "SET --");
    lv_obj_set_style_text_color(h.lbl_temp_setpoint, cockpit_color_dim(), 0);
    lv_obj_set_style_text_font(h.lbl_temp_setpoint, cockpit_font_small(), 0);
    lv_obj_align(h.lbl_temp_setpoint, LV_ALIGN_CENTER, 0, 42);

    h.lbl_heater_relay = lv_label_create(h.gauge_temp.root);
    lv_label_set_text(h.lbl_heater_relay, "RELAY --");
    lv_obj_set_style_text_color(h.lbl_heater_relay, cockpit_color_amber(), 0);
    lv_obj_set_style_text_font(h.lbl_heater_relay, cockpit_font_small(), 0);
    lv_obj_align(h.lbl_heater_relay, LV_ALIGN_BOTTOM_MID, 0, -36);

    h.lbl_heater_shelly = lv_label_create(h.gauge_temp.root);
    lv_label_set_text(h.lbl_heater_shelly, "SHELLY --");
    lv_obj_set_style_text_color(h.lbl_heater_shelly, cockpit_color_dim(), 0);
    lv_obj_set_style_text_font(h.lbl_heater_shelly, cockpit_font_small(), 0);
    lv_obj_align(h.lbl_heater_shelly, LV_ALIGN_BOTTOM_MID, 0, -52);

    cockpit_gauge_create(&h.gauge_co2, parent, 356, 52, 118, "CO2", "INJECTION",
                         GAUGE_START, GAUGE_END, 0.0f, 1.0f);
    cockpit_gauge_set_scale_ticks(&h.gauge_co2, "OFF", "ON");
    lv_obj_add_flag(h.gauge_co2.root, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(h.gauge_co2.root, tap_co2_cb, LV_EVENT_CLICKED, NULL);

    cockpit_gauge_create(&h.gauge_feeder, parent, 6, 302, 104, "FEEDER", "SCHEDULE",
                         GAUGE_START, GAUGE_END, 0.0f, 1.0f);
    lv_obj_add_flag(h.gauge_feeder.root, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(h.gauge_feeder.root, tap_feeder_cb, LV_EVENT_CLICKED, NULL);

    h.panel_trend = make_panel(parent, 116, 302, 356, 104);
    panel_title(h.panel_trend, "TEMP TREND 24H");
    h.lbl_trend_min = lv_label_create(h.panel_trend);
    lv_label_set_text(h.lbl_trend_min, "MIN --");
    lv_obj_set_style_text_color(h.lbl_trend_min, cockpit_color_dim(), 0);
    lv_obj_set_style_text_font(h.lbl_trend_min, cockpit_font_small(), 0);
    lv_obj_align(h.lbl_trend_min, LV_ALIGN_TOP_RIGHT, 0, 0);
    h.lbl_trend_max = lv_label_create(h.panel_trend);
    lv_label_set_text(h.lbl_trend_max, "MAX --");
    lv_obj_set_style_text_color(h.lbl_trend_max, cockpit_color_dim(), 0);
    lv_obj_set_style_text_font(h.lbl_trend_max, cockpit_font_small(), 0);
    lv_obj_align(h.lbl_trend_max, LV_ALIGN_TOP_RIGHT, 0, 14);

    h.chart_temp = lv_chart_create(h.panel_trend);
    lv_obj_set_size(h.chart_temp, 340, 68);
    lv_obj_align(h.chart_temp, LV_ALIGN_BOTTOM_MID, 0, -2);
    lv_obj_set_style_bg_color(h.chart_temp, lv_color_hex(0x0c1218), 0);
    lv_obj_set_style_border_color(h.chart_temp, lv_color_hex(0x334455), 0);
    lv_obj_set_style_border_width(h.chart_temp, 1, 0);
    lv_chart_set_type(h.chart_temp, LV_CHART_TYPE_LINE);
    lv_chart_set_range(h.chart_temp, LV_CHART_AXIS_PRIMARY_Y, 0, 100);
    lv_chart_set_point_count(h.chart_temp, DASHBOARD_TEMP_HISTORY_LEN);
    h.chart_temp_series = lv_chart_add_series(h.chart_temp, cockpit_color_cyan(), LV_CHART_AXIS_PRIMARY_Y);

    h.panel_water = make_panel(parent, 6, 414, 150, 92);
    panel_title(h.panel_water, "WATER");
    lv_obj_add_flag(h.panel_water, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(h.panel_water, tap_water_cb, LV_EVENT_CLICKED, NULL);
    make_water_column(h.panel_water, 4, "pH", &h.lbl_water_ph, &h.badge_water_ph);
    make_water_column(h.panel_water, 38, "NH3", &h.lbl_water_nh3, &h.badge_water_nh3);
    make_water_column(h.panel_water, 72, "NO2", &h.lbl_water_no2, &h.badge_water_no2);
    make_water_column(h.panel_water, 106, "NO3", &h.lbl_water_no3, &h.badge_water_no3);
    h.lbl_water_updated = lv_label_create(h.panel_water);
    lv_label_set_text(h.lbl_water_updated, "Updated --");
    lv_obj_set_style_text_color(h.lbl_water_updated, cockpit_color_dim(), 0);
    lv_obj_set_style_text_font(h.lbl_water_updated, cockpit_font_small(), 0);
    lv_obj_align(h.lbl_water_updated, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    h.btn_water_view = make_text_btn(h.panel_water, "VIEW ALL", tap_water_cb);
    lv_obj_set_size(h.btn_water_view, 72, 22);
    lv_obj_align(h.btn_water_view, LV_ALIGN_BOTTOM_RIGHT, 0, 0);

    h.panel_reminders = make_panel(parent, 162, 414, 150, 92);
    panel_title(h.panel_reminders, "REMINDERS");
    lv_obj_add_flag(h.panel_reminders, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(h.panel_reminders, tap_reminders_cb, LV_EVENT_CLICKED, NULL);
    h.lbl_reminders_due = lv_label_create(h.panel_reminders);
    lv_label_set_text(h.lbl_reminders_due, "0 DUE");
    lv_obj_set_style_text_color(h.lbl_reminders_due, cockpit_color_text(), 0);
    lv_obj_set_style_text_font(h.lbl_reminders_due, cockpit_font_value(), 0);
    lv_obj_align(h.lbl_reminders_due, LV_ALIGN_LEFT_MID, 0, -8);
    h.lbl_reminders_next = lv_label_create(h.panel_reminders);
    lv_label_set_text(h.lbl_reminders_next, "NEXT: --");
    lv_obj_set_style_text_color(h.lbl_reminders_next, cockpit_color_dim(), 0);
    lv_obj_set_style_text_font(h.lbl_reminders_next, cockpit_font_small(), 0);
    lv_obj_align(h.lbl_reminders_next, LV_ALIGN_BOTTOM_LEFT, 0, 18);
    h.btn_reminders_view = make_text_btn(h.panel_reminders, "VIEW ALL", tap_reminders_cb);
    lv_obj_set_size(h.btn_reminders_view, 72, 22);
    lv_obj_align(h.btn_reminders_view, LV_ALIGN_BOTTOM_RIGHT, 0, 0);

    h.panel_clock = make_panel(parent, 318, 414, 154, 92);
    panel_title(h.panel_clock, "CLOCK");
    h.lbl_clock_time = lv_label_create(h.panel_clock);
    lv_label_set_text(h.lbl_clock_time, "--:--");
    lv_obj_set_style_text_color(h.lbl_clock_time, cockpit_color_text(), 0);
    lv_obj_set_style_text_font(h.lbl_clock_time, cockpit_font_value(), 0);
    lv_obj_align(h.lbl_clock_time, LV_ALIGN_LEFT_MID, 0, -10);
    h.lbl_clock_date = lv_label_create(h.panel_clock);
    lv_label_set_text(h.lbl_clock_date, "---");
    lv_obj_set_style_text_color(h.lbl_clock_date, cockpit_color_dim(), 0);
    lv_obj_set_style_text_font(h.lbl_clock_date, cockpit_font_small(), 0);
    lv_obj_align(h.lbl_clock_date, LV_ALIGN_BOTTOM_LEFT, 0, 18);
    h.badge_mode = lv_obj_create(h.panel_clock);
    lv_obj_remove_style_all(h.badge_mode);
    lv_obj_set_size(h.badge_mode, 64, 22);
    lv_obj_set_style_bg_color(h.badge_mode, lv_color_hex(0x0d3320), 0);
    lv_obj_set_style_bg_opa(h.badge_mode, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(h.badge_mode, 6, 0);
    lv_obj_align(h.badge_mode, LV_ALIGN_TOP_RIGHT, 0, 16);
    lv_obj_t *mode_lbl = lv_label_create(h.badge_mode);
    lv_label_set_text(mode_lbl, "AUTO");
    lv_obj_set_style_text_color(mode_lbl, cockpit_color_green(), 0);
    lv_obj_set_style_text_font(mode_lbl, cockpit_font_small(), 0);
    lv_obj_center(mode_lbl);

    h.panel_systems = make_panel(parent, 6, 514, 232, 108);
    panel_title(h.panel_systems, "SYSTEMS");
    struct {
        const char *name;
        lv_coord_t x;
        lv_coord_t y;
        lv_obj_t **led;
    } sys_leds[] = {
        {"Wi-Fi", 0, 20, &h.led_wifi},
        {"Shelly CO2", 0, 38, &h.led_co2},
        {"Filter", 0, 56, &h.led_filter},
        {"Heater", 0, 74, &h.led_heater},
        {"BLE Central", 116, 20, &h.led_ble},
        {"Feeder", 116, 38, &h.led_feeder},
        {"Lighting", 116, 56, &h.led_light},
        {"Alerts", 116, 74, &h.led_alerts},
    };
    for (size_t i = 0; i < sizeof(sys_leds) / sizeof(sys_leds[0]); i++) {
        lv_obj_t *lbl = lv_label_create(h.panel_systems);
        lv_label_set_text(lbl, sys_leds[i].name);
        lv_obj_set_style_text_color(lbl, cockpit_color_dim(), 0);
        lv_obj_set_style_text_font(lbl, cockpit_font_small(), 0);
        lv_obj_set_pos(lbl, sys_leds[i].x, sys_leds[i].y);
        lv_obj_t *led = lv_led_create(h.panel_systems);
        lv_obj_set_size(led, 12, 12);
        lv_obj_set_pos(led, sys_leds[i].x + 92, sys_leds[i].y + 2);
        lv_led_off(led);
        *sys_leds[i].led = led;
    }

    h.panel_light = make_panel(parent, 244, 514, 228, 108);
    panel_title(h.panel_light, "LIGHTING");
    lv_obj_add_flag(h.panel_light, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(h.panel_light, tap_light_cb, LV_EVENT_CLICKED, NULL);
    h.lbl_light_mode = lv_label_create(h.panel_light);
    lv_label_set_text(h.lbl_light_mode, "DAYLIGHT");
    lv_obj_set_style_text_color(h.lbl_light_mode, cockpit_color_text(), 0);
    lv_obj_set_style_text_font(h.lbl_light_mode, cockpit_font_body(), 0);
    lv_obj_align(h.lbl_light_mode, LV_ALIGN_TOP_LEFT, 0, 18);
    h.bar_light = lv_bar_create(h.panel_light);
    lv_obj_set_size(h.bar_light, 200, 10);
    lv_obj_align(h.bar_light, LV_ALIGN_LEFT_MID, 0, 6);
    lv_bar_set_range(h.bar_light, 0, 100);
    lv_obj_set_style_bg_color(h.bar_light, lv_color_hex(0x1a2838), LV_PART_MAIN);
    lv_obj_set_style_bg_color(h.bar_light, cockpit_color_cyan(), LV_PART_INDICATOR);
    h.lbl_light_sunrise = lv_label_create(h.panel_light);
    lv_label_set_text(h.lbl_light_sunrise, "SUN --:--");
    lv_obj_set_style_text_color(h.lbl_light_sunrise, cockpit_color_dim(), 0);
    lv_obj_set_style_text_font(h.lbl_light_sunrise, cockpit_font_small(), 0);
    lv_obj_align(h.lbl_light_sunrise, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    h.lbl_light_sunset = lv_label_create(h.panel_light);
    lv_label_set_text(h.lbl_light_sunset, "SET --:--");
    lv_obj_set_style_text_color(h.lbl_light_sunset, cockpit_color_dim(), 0);
    lv_obj_set_style_text_font(h.lbl_light_sunset, cockpit_font_small(), 0);
    lv_obj_align(h.lbl_light_sunset, LV_ALIGN_BOTTOM_RIGHT, 0, 0);

    h.nav_bar = lv_obj_create(parent);
    cockpit_style_apply_nav_bar(h.nav_bar);
    lv_obj_set_size(h.nav_bar, 468, 50);
    lv_obj_set_pos(h.nav_bar, 6, 632);
    lv_obj_set_flex_flow(h.nav_bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(h.nav_bar, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_remove_flag(h.nav_bar, LV_OBJ_FLAG_SCROLLABLE);

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
        h.nav_btns[i] = lv_btn_create(h.nav_bar);
        lv_obj_set_size(h.nav_btns[i], 72, 40);
        lv_obj_set_style_bg_color(h.nav_btns[i], lv_color_hex(0x141c24), 0);
        lv_obj_set_style_border_color(h.nav_btns[i], lv_color_hex(0x334455), 0);
        lv_obj_set_style_border_width(h.nav_btns[i], 1, 0);
        if (nav_items[i].cb) {
            lv_obj_add_event_cb(h.nav_btns[i], nav_items[i].cb, LV_EVENT_CLICKED, NULL);
        }
        lv_obj_t *lbl = lv_label_create(h.nav_btns[i]);
        lv_label_set_text(lbl, nav_items[i].label);
        lv_obj_set_style_text_font(lbl, cockpit_font_small(), 0);
        lv_obj_set_style_text_color(lbl, cockpit_color_cyan(), 0);
        lv_obj_center(lbl);
    }

    s_handles = h;
    return h;
}

static void update_temp_chart(fishduino_cockpit_handles_t *h, const dashboard_snapshot_t *s)
{
    if (h->chart_temp == NULL || h->chart_temp_series == NULL) {
        return;
    }
    uint8_t cnt = s->temp_history_count;
    if (cnt == 0) {
        lv_chart_set_point_count(h->chart_temp, 1);
        lv_chart_set_all_value(h->chart_temp, h->chart_temp_series, LV_CHART_POINT_NONE);
        lv_chart_refresh(h->chart_temp);
        return;
    }
    lv_chart_set_point_count(h->chart_temp, cnt);
    float min = s->temp_trend_valid ? s->temp_trend_min_f : s->temp_history[0];
    float max = s->temp_trend_valid ? s->temp_trend_max_f : s->temp_history[0];
    for (uint8_t i = 1; i < cnt; i++) {
        if (s->temp_history[i] < min) {
            min = s->temp_history[i];
        }
        if (s->temp_history[i] > max) {
            max = s->temp_history[i];
        }
    }
    float span = max - min;
    if (span < 0.3f) {
        span = 0.3f;
    }
    for (uint8_t i = 0; i < cnt; i++) {
        int32_t yv = (int32_t)(((s->temp_history[i] - min) / span) * 100.0f);
        if (yv < 0) {
            yv = 0;
        }
        if (yv > 100) {
            yv = 100;
        }
        lv_chart_set_value_by_id(h->chart_temp, h->chart_temp_series, i, yv);
    }
    lv_chart_refresh(h->chart_temp);
}

#define TEMP_GAUGE_MIN_F 70.0f
#define TEMP_GAUGE_MAX_F 90.0f
#define TEMP_GAUGE_FALLBACK_SETPOINT_F 78.0f

static bool gauge_setpoint_valid(const dashboard_snapshot_t *s)
{
    return s->heater_enabled && s->setpoint_f >= 50.0f && s->setpoint_f <= 95.0f;
}

static float gauge_display_setpoint_f(const dashboard_snapshot_t *s)
{
    if (!gauge_setpoint_valid(s)) {
        return TEMP_GAUGE_FALLBACK_SETPOINT_F;
    }
    return s->setpoint_f;
}

static void update_temp_gauge(fishduino_cockpit_handles_t *h, const dashboard_snapshot_t *s)
{
    char val[16];
    char sub[32];
    char st[24];

    float setpt = gauge_display_setpoint_f(s);
    float min_val = TEMP_GAUGE_MIN_F;
    float max_val = TEMP_GAUGE_MAX_F;
    if (min_val >= max_val) {
        min_val = 70.0f;
        max_val = 90.0f;
    }
    h->gauge_temp.min_val = min_val;
    h->gauge_temp.max_val = max_val;
    cockpit_gauge_set_scale_ticks(&h->gauge_temp, "70", "90");

    if (s->temp_valid) {
        snprintf(val, sizeof(val), "%.1f", (double)s->temp_f);
    } else {
        snprintf(val, sizeof(val), "--");
    }
    snprintf(sub, sizeof(sub), "Tank F");
    snprintf(st, sizeof(st), "%s", s->heater_state_text);
    cockpit_gauge_set_labels(&h->gauge_temp, val, sub, st);

    if (gauge_setpoint_valid(s)) {
        cockpit_gauge_set_zones_linear(&h->gauge_temp, s->setpoint_f - 0.5f, s->setpoint_f + 0.5f);
    } else {
        cockpit_gauge_set_zones_linear(&h->gauge_temp, setpt - 0.5f, setpt + 0.5f);
    }
    if (s->heater_enabled && s->temp_valid) {
        cockpit_gauge_set_needle(&h->gauge_temp, s->temp_f, true);
    } else {
        cockpit_gauge_set_needle(&h->gauge_temp, setpt, false);
    }

    char setpt_lbl[24];
    snprintf(setpt_lbl, sizeof(setpt_lbl), "SET %.1fF", (double)setpt);
    lv_label_set_text(h->lbl_temp_setpoint, setpt_lbl);

    lv_color_t chihiros_color = cockpit_color_amber();
    if (!s->heater_enabled) {
        chihiros_color = cockpit_color_dim();
    } else if (!s->heater_online || s->temp_stale) {
        chihiros_color = cockpit_color_red();
    } else if (s->heater_heating) {
        chihiros_color = cockpit_color_red();
    } else {
        chihiros_color = cockpit_color_green();
    }
    lv_label_set_text(h->lbl_heater_relay, s->heater_relay_text);
    lv_obj_set_style_text_color(h->lbl_heater_relay, chihiros_color, 0);

    lv_color_t shelly_color = cockpit_color_dim();
    if (!s->heater_shelly_enabled) {
        shelly_color = cockpit_color_dim();
    } else if (!s->heater_shelly_online || s->heater_shelly_stale) {
        shelly_color = cockpit_color_red();
    } else if (s->heater_shelly_relay_on) {
        shelly_color = cockpit_color_green();
    } else {
        shelly_color = cockpit_color_amber();
    }
    lv_label_set_text(h->lbl_heater_shelly, s->heater_shelly_text);
    lv_obj_set_style_text_color(h->lbl_heater_shelly, shelly_color, 0);
}

static void update_filter_gauge(fishduino_cockpit_handles_t *h, const dashboard_snapshot_t *s)
{
    char val[16];
    char sub[32];

    float max_w = s->filter_baseline_watts > 0.0f ? s->filter_baseline_watts * 2.0f : 25.0f;
    if (max_w < 10.0f) {
        max_w = 10.0f;
    }
    h->gauge_filter.min_val = 0.0f;
    h->gauge_filter.max_val = max_w;

    snprintf(val, sizeof(val), "%.1f", (double)s->filter_watts);
    if (s->filter_baseline_watts > 0.0f) {
        snprintf(sub, sizeof(sub), "%.1fW avg", (double)s->filter_baseline_watts);
    } else {
        snprintf(sub, sizeof(sub), "WATTS");
    }
    cockpit_gauge_set_labels(&h->gauge_filter, val, sub, s->filter_health_text);

    float lo = s->filter_threshold_watts > 0.0f ? s->filter_threshold_watts : (s->filter_baseline_watts * 0.5f);
    float hi = s->filter_baseline_watts > 0.0f ? s->filter_baseline_watts * 1.5f : max_w;
    if (lo <= 0.0f) {
        lo = 1.0f;
    }
    cockpit_gauge_set_zones_linear(&h->gauge_filter, lo, hi);
    cockpit_gauge_set_needle(&h->gauge_filter, s->filter_watts, true);

    lv_color_t c = cockpit_color_green();
    if (s->filter_alarm == FISHDUINO_FILTER_ALARM_OFFLINE || s->filter_alarm == FISHDUINO_FILTER_ALARM_OFF) {
        c = cockpit_color_red();
    } else if (s->filter_alarm == FISHDUINO_FILTER_ALARM_LOW_POWER) {
        c = cockpit_color_amber();
    }
    lv_obj_set_style_text_color(h->gauge_filter.lbl_status, c, 0);
}

static void update_co2_gauge(fishduino_cockpit_handles_t *h, const dashboard_snapshot_t *s)
{
    bool relay = s->co2_relay_known ? s->co2_relay_on : false;
    cockpit_gauge_set_zones_on_off(&h->gauge_co2, relay);
    cockpit_gauge_set_needle(&h->gauge_co2, relay ? 1.0f : 0.0f, true);
    cockpit_gauge_set_labels(&h->gauge_co2, s->co2_state_text, "INJECTION", s->co2_block_text);

    lv_color_t c = cockpit_color_red();
    if (!s->co2_online && s->co2_enabled) {
        c = cockpit_color_red();
    } else if (s->co2_relay_known && s->co2_desired_on == s->co2_relay_on) {
        c = s->co2_relay_on ? cockpit_color_green() : cockpit_color_amber();
    } else if (s->co2_desired_on) {
        c = cockpit_color_amber();
    }
    if (s->co2_block_reason != CO2_BLOCK_NONE) {
        c = cockpit_color_amber();
    }
    lv_obj_set_style_text_color(h->gauge_co2.lbl_value, c, 0);
    lv_obj_set_style_text_color(h->gauge_co2.lbl_status, c, 0);
}

static void update_feeder_gauge(fishduino_cockpit_handles_t *h, const dashboard_snapshot_t *s)
{
    const char *en = "OFF";
    if (!s->feeder_configured) {
        en = "NO GPIO";
    } else if (s->feeder_scheduled) {
        en = "ON";
    } else {
        en = "OFF";
    }

    char sub[28];
    if (s->next_feed_text[0] != '\0') {
        snprintf(sub, sizeof(sub), "NEXT %s", s->next_feed_text);
    } else {
        snprintf(sub, sizeof(sub), "NEXT --");
    }

    const char *status = "READY";
    if (!s->feeder_configured) {
        status = "NO GPIO";
    } else if (!s->feeder_scheduled) {
        status = "DISABLED";
    }

    cockpit_gauge_set_zones_on_off(&h->gauge_feeder, s->feeder_scheduled && s->feeder_configured);
    cockpit_gauge_set_needle(&h->gauge_feeder,
                             (s->feeder_scheduled && s->feeder_configured) ? 1.0f : 0.0f, true);
    cockpit_gauge_set_labels(&h->gauge_feeder, en, sub, status);
}

static void update_water_panel(fishduino_cockpit_handles_t *h, const dashboard_snapshot_t *s)
{
    const water_test_entry_t *we = &s->water_latest;
    char buf[12];

    if (s->water_has_entry && (we->valid_flags & WATER_VALID_PH)) {
        snprintf(buf, sizeof(buf), "%.1f", (double)we->ph);
    } else {
        snprintf(buf, sizeof(buf), "--");
    }
    lv_label_set_text(h->lbl_water_ph, buf);
    style_badge(h->badge_water_ph, s->water_ph_ok, !s->water_has_entry);

    if (s->water_has_entry && (we->valid_flags & WATER_VALID_AMMONIA)) {
        snprintf(buf, sizeof(buf), "%.1f", (double)we->ammonia_ppm);
    } else {
        snprintf(buf, sizeof(buf), "--");
    }
    lv_label_set_text(h->lbl_water_nh3, buf);
    style_badge(h->badge_water_nh3, s->water_nh3_ok, !s->water_has_entry);

    if (s->water_has_entry && (we->valid_flags & WATER_VALID_NITRITE)) {
        snprintf(buf, sizeof(buf), "%.1f", (double)we->nitrite_ppm);
    } else {
        snprintf(buf, sizeof(buf), "--");
    }
    lv_label_set_text(h->lbl_water_no2, buf);
    style_badge(h->badge_water_no2, s->water_no2_ok, !s->water_has_entry);

    if (s->water_has_entry && (we->valid_flags & WATER_VALID_NITRATE)) {
        snprintf(buf, sizeof(buf), "%.0f", (double)we->nitrate_ppm);
    } else {
        snprintf(buf, sizeof(buf), "--");
    }
    lv_label_set_text(h->lbl_water_no3, buf);
    style_badge(h->badge_water_no3, s->water_no3_ok, !s->water_has_entry);

    if (s->water_has_entry && s->water_updated_text[0]) {
        char upd[40];
        snprintf(upd, sizeof(upd), "%s", s->water_updated_text);
        lv_label_set_text(h->lbl_water_updated, upd);
    } else {
        lv_label_set_text(h->lbl_water_updated, "No test logged");
    }
}

void fishduino_cockpit_dashboard_update(fishduino_cockpit_handles_t *h, const dashboard_snapshot_t *s)
{
    if (h == NULL || s == NULL) {
        return;
    }

    if (s->banner_visible) {
        lv_label_set_text(h->lbl_banner, s->banner_text);
        lv_obj_remove_flag(h->banner, LV_OBJ_FLAG_HIDDEN);
        lv_color_t bc = s->banner_critical ? lv_color_hex(0x441111) : lv_color_hex(0x332211);
        lv_obj_set_style_bg_color(h->banner, bc, 0);
    } else {
        lv_obj_add_flag(h->banner, LV_OBJ_FLAG_HIDDEN);
    }

    update_temp_gauge(h, s);
    update_filter_gauge(h, s);
    update_co2_gauge(h, s);
    update_feeder_gauge(h, s);
    update_temp_chart(h, s);

    if (s->temp_trend_valid) {
        char t1[24], t2[24];
        snprintf(t1, sizeof(t1), "MIN %.1f", (double)s->temp_trend_min_f);
        snprintf(t2, sizeof(t2), "MAX %.1f", (double)s->temp_trend_max_f);
        lv_label_set_text(h->lbl_trend_min, t1);
        lv_label_set_text(h->lbl_trend_max, t2);
    } else {
        lv_label_set_text(h->lbl_trend_min, "MIN --");
        lv_label_set_text(h->lbl_trend_max, "MAX --");
    }

    update_water_panel(h, s);

    if (s->reminders_any) {
        char due[24];
        snprintf(due, sizeof(due), "%zu DUE", s->reminders_due_count);
        lv_label_set_text(h->lbl_reminders_due, due);
        char next[56];
        snprintf(next, sizeof(next), "NEXT: %s", s->next_reminder_name);
        lv_label_set_text(h->lbl_reminders_next, next);
        lv_color_t rc = s->next_reminder_status == MAINT_TRACKER_STATUS_OVERDUE ? cockpit_color_red()
                                                                                : cockpit_color_amber();
        lv_obj_set_style_text_color(h->lbl_reminders_due, rc, 0);
    } else {
        lv_label_set_text(h->lbl_reminders_due, "0 DUE");
        lv_label_set_text(h->lbl_reminders_next, "ALL CLEAR");
        lv_obj_set_style_text_color(h->lbl_reminders_due, cockpit_color_green(), 0);
    }

    lv_label_set_text(h->lbl_clock_time, s->clock_time);
    lv_label_set_text(h->lbl_clock_date, s->clock_date);
    lv_obj_t *mode_lbl = lv_obj_get_child(h->badge_mode, 0);
    if (mode_lbl) {
        lv_label_set_text(mode_lbl, s->mode_text);
        lv_color_t mc = cockpit_color_green();
        if (strcmp(s->mode_text, "MAINT") == 0 || strcmp(s->mode_text, "MANUAL") == 0) {
            mc = cockpit_color_amber();
        } else if (strcmp(s->mode_text, "SCHED OFF") == 0) {
            mc = cockpit_color_red();
        }
        lv_obj_set_style_text_color(mode_lbl, mc, 0);
        if (strcmp(s->mode_text, "AUTO") == 0) {
            lv_obj_set_style_bg_color(h->badge_mode, lv_color_hex(0x0d3320), 0);
        } else {
            lv_obj_set_style_bg_color(h->badge_mode, lv_color_hex(0x3d2a10), 0);
        }
    }

    apply_led(h->led_wifi, s->led_wifi);
    apply_led(h->led_co2, s->led_shelly_co2);
    apply_led(h->led_filter, s->led_shelly_filter);
    apply_led(h->led_heater, s->led_shelly_heater);
    apply_led(h->led_ble, s->led_ble);
    apply_led(h->led_feeder, s->led_feeder);
    apply_led(h->led_light, s->led_light);
    apply_led(h->led_alerts, s->led_alerts);

    lv_label_set_text(h->lbl_light_mode, s->light_mode_label);
    lv_bar_set_value(h->bar_light, s->light_intensity_pct, LV_ANIM_OFF);
    char sr[24], ss[24];
    snprintf(sr, sizeof(sr), "SUN %s", s->light_sunrise_text);
    snprintf(ss, sizeof(ss), "SET %s", s->light_sunset_text);
    lv_label_set_text(h->lbl_light_sunrise, sr);
    lv_label_set_text(h->lbl_light_sunset, ss);
}

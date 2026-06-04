#include "screen_dashboard_cockpit.h"

#include <stdio.h>
#include <string.h>

#include "esp_log.h"

#include "screen_co2_schedule.h"
#include "screen_fluval_settings.h"
#include "screen_heater_settings.h"
#include "screen_maint_tracker.h"
#include "screen_options.h"
#include "screen_water_tests.h"

#define GAUGE_START 135
#define GAUGE_END   405
#define NAV_COUNT   6

#ifndef FISHDUINO_COCKPIT_WIDE_LAYOUT_DEBUG
#define FISHDUINO_COCKPIT_WIDE_LAYOUT_DEBUG 1
#endif

/* 7B cockpit-cluster layout (1024×600), inspired by industrial HMI reference. */
#define W7_SCREEN_W           1024
#define W7_SCREEN_H           600
#define W7_MARGIN             8
#define W7_INNER_W            (W7_SCREEN_W - (W7_MARGIN * 2))

#define W7_HEADER_Y           4
#define W7_HEADER_H           32
#define W7_BRAND_Y            30

#define W7_SIDE_GAUGE_Y       54
#define W7_TEMP_GAUGE_Y       40
#define W7_FILTER_GAUGE_SZ    200
#define W7_TEMP_GAUGE_SZ      300
#define W7_CO2_GAUGE_SZ       200

#define W7_GX_FILTER          24
#define W7_GX_TEMP            362
#define W7_GX_CO2             800

#define W7_FEEDER_GAUGE_X     92
#define W7_FEEDER_GAUGE_Y     258
#define W7_FEEDER_GAUGE_SZ    136

#define W7_TREND_PANEL_X      784
#define W7_TREND_PANEL_Y      272
#define W7_TREND_PANEL_W      180
#define W7_TREND_PANEL_H      150

#define W7_CARD_ROW_Y         398
#define W7_CARD_ROW_H         134

#define W7_WATER_X            8
#define W7_WATER_W            248

#define W7_REMINDERS_X        264
#define W7_REMINDERS_W        144

#define W7_CLOCK_X            416
#define W7_CLOCK_W            192

#define W7_SYSTEMS_X          616
#define W7_SYSTEMS_W          200

#define W7_LIGHT_X            824
#define W7_LIGHT_W            192

#define W7_NAV_Y              544
#define W7_NAV_H              48
#define W7_MIN_TOUCH_H        40

#define W7_GAUGE_ROOT_H(sz)  ((sz) + 40)

/* Procedural cockpit background (no bitmap). */
#define W7_BG_COLOR          0x070A0F
#define W7_BG_PANEL_COLOR    0x0B1017
#define W7_BG_FRAME_DARK     0x030507
#define W7_BG_LINE_COLOR     0x16202A
#define W7_BG_ACCENT_BLUE    0x00AEEF
#define W7_BG_BADGE_COLOR    0x121821
#define W7_BG_SCREW_COLOR    0x1B222B
#define W7_BG_BORDER_LITE    0x26313C
#define W7_BG_BORDER_PANEL   0x1D2A36
#define W7_BG_SEAM_COLOR     0x0A1018

#define W7_FRAME_X           0
#define W7_FRAME_Y           0
#define W7_FRAME_W           W7_SCREEN_W
#define W7_FRAME_H           W7_SCREEN_H

#define W7_INNER_PANEL_X     8
#define W7_INNER_PANEL_Y     38
#define W7_INNER_PANEL_W     1008
#define W7_INNER_PANEL_H     496

#define W7_HEADER_BADGE_W    300
#define W7_HEADER_BADGE_H    34
#define W7_HEADER_BADGE_X    ((W7_SCREEN_W - W7_HEADER_BADGE_W) / 2)
#define W7_HEADER_BADGE_Y    2

#define W7_RIB_INSET_X       16
#define W7_RIB_Y_START       52
#define W7_RIB_Y_END         526
#define W7_RIB_SPACING       12

#define W7_SEAM_CARD_Y       390
#define W7_SEAM_NAV_Y        538
#define W7_SEAM_SIDE_X_L     8
#define W7_SEAM_SIDE_X_R     1016

#define W7_SCREW_SZ          14
#define W7_SCREW_TL_X        16
#define W7_SCREW_TL_Y        16
#define W7_SCREW_TR_X        1008
#define W7_SCREW_TR_Y        16
#define W7_SCREW_BL_X        16
#define W7_SCREW_BL_Y        584
#define W7_SCREW_BR_X        1008
#define W7_SCREW_BR_Y        584

typedef enum {
    W7_RECT_ROLE_DECORATIVE,
    W7_RECT_ROLE_MAIN_GAUGE,
    W7_RECT_ROLE_TUCKED_GAUGE,
    W7_RECT_ROLE_CARD,
    W7_RECT_ROLE_NAV,
    W7_RECT_ROLE_TOUCH,
} w7_rect_role_t;

static const char *TAG_WIDE = "cockpit_wide";

typedef struct {
    const char *name;
    lv_coord_t x;
    lv_coord_t y;
    lv_coord_t w;
    lv_coord_t h;
    w7_rect_role_t role;
} cockpit_wide_rect_t;

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

static void decor_no_input(lv_obj_t *obj)
{
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_CLICKABLE);
}

static lv_obj_t *make_seam_line(lv_obj_t *parent, lv_coord_t x, lv_coord_t y, lv_coord_t w,
                                lv_coord_t h)
{
    lv_obj_t *line = lv_obj_create(parent);
    lv_obj_remove_style_all(line);
    lv_obj_set_pos(line, x, y);
    lv_obj_set_size(line, w, h);
    lv_obj_set_style_bg_color(line, lv_color_hex(W7_BG_SEAM_COLOR), 0);
    lv_obj_set_style_bg_opa(line, LV_OPA_30, 0);
    decor_no_input(line);
    return line;
}

static lv_obj_t *make_screw(lv_obj_t *parent, lv_coord_t cx, lv_coord_t cy)
{
    lv_obj_t *s = lv_obj_create(parent);
    lv_obj_remove_style_all(s);
    lv_obj_set_size(s, W7_SCREW_SZ, W7_SCREW_SZ);
    lv_obj_set_pos(s, cx - (W7_SCREW_SZ / 2), cy - (W7_SCREW_SZ / 2));
    lv_obj_set_style_radius(s, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(s, lv_color_hex(W7_BG_SCREW_COLOR), 0);
    lv_obj_set_style_bg_opa(s, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(s, lv_color_hex(0x485461), 0);
    lv_obj_set_style_border_width(s, 1, 0);
    decor_no_input(s);

    lv_obj_t *slot = lv_obj_create(s);
    lv_obj_remove_style_all(slot);
    lv_obj_set_size(slot, 8, 2);
    lv_obj_set_style_bg_color(slot, lv_color_hex(0x05070A), 0);
    lv_obj_set_style_bg_opa(slot, LV_OPA_COVER, 0);
    lv_obj_center(slot);
    decor_no_input(slot);
    return s;
}

static void build_w7_background(lv_obj_t *parent)
{
    lv_obj_t *frame = lv_obj_create(parent);
    lv_obj_remove_style_all(frame);
    lv_obj_set_pos(frame, W7_FRAME_X, W7_FRAME_Y);
    lv_obj_set_size(frame, W7_FRAME_W, W7_FRAME_H);
    lv_obj_set_style_bg_color(frame, lv_color_hex(W7_BG_FRAME_DARK), 0);
    lv_obj_set_style_bg_opa(frame, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(frame, lv_color_hex(W7_BG_BORDER_LITE), 0);
    lv_obj_set_style_border_width(frame, 2, 0);
    lv_obj_set_style_radius(frame, 12, 0);
    decor_no_input(frame);

    lv_obj_t *inner = lv_obj_create(parent);
    lv_obj_remove_style_all(inner);
    lv_obj_set_pos(inner, W7_INNER_PANEL_X, W7_INNER_PANEL_Y);
    lv_obj_set_size(inner, W7_INNER_PANEL_W, W7_INNER_PANEL_H);
    lv_obj_set_style_bg_color(inner, lv_color_hex(W7_BG_PANEL_COLOR), 0);
    lv_obj_set_style_bg_opa(inner, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(inner, lv_color_hex(W7_BG_BORDER_PANEL), 0);
    lv_obj_set_style_border_width(inner, 1, 0);
    lv_obj_set_style_radius(inner, 16, 0);
    lv_obj_set_style_shadow_color(inner, lv_color_hex(0x000000), 0);
    lv_obj_set_style_shadow_width(inner, 8, 0);
    lv_obj_set_style_shadow_opa(inner, LV_OPA_40, 0);
    lv_obj_set_style_shadow_ofs_y(inner, 2, 0);
    decor_no_input(inner);

    const lv_coord_t rib_w = W7_INNER_PANEL_W - (W7_RIB_INSET_X * 2);
    const lv_coord_t rib_x = W7_RIB_INSET_X;
    for (lv_coord_t y = W7_RIB_Y_START; y <= W7_RIB_Y_END; y += W7_RIB_SPACING) {
        const lv_coord_t ry = y - W7_INNER_PANEL_Y;
        lv_obj_t *rib = lv_obj_create(inner);
        lv_obj_remove_style_all(rib);
        lv_obj_set_pos(rib, rib_x, ry);
        lv_obj_set_size(rib, rib_w, 1);
        lv_obj_set_style_bg_color(rib, lv_color_hex(W7_BG_LINE_COLOR), 0);
        lv_obj_set_style_bg_opa(rib, LV_OPA_20, 0);
        decor_no_input(rib);
    }

    make_seam_line(parent, W7_MARGIN, W7_SEAM_CARD_Y, W7_INNER_W, 1);
    make_seam_line(parent, W7_MARGIN, W7_SEAM_NAV_Y, W7_INNER_W, 1);
    make_seam_line(parent, W7_SEAM_SIDE_X_L, W7_INNER_PANEL_Y, 1, W7_INNER_PANEL_H);
    make_seam_line(parent, W7_SEAM_SIDE_X_R, W7_INNER_PANEL_Y, 1, W7_INNER_PANEL_H);

    lv_obj_t *badge = lv_obj_create(parent);
    lv_obj_remove_style_all(badge);
    lv_obj_set_pos(badge, W7_HEADER_BADGE_X, W7_HEADER_BADGE_Y);
    lv_obj_set_size(badge, W7_HEADER_BADGE_W, W7_HEADER_BADGE_H);
    lv_obj_set_style_bg_color(badge, lv_color_hex(W7_BG_BADGE_COLOR), 0);
    lv_obj_set_style_bg_opa(badge, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(badge, lv_color_hex(W7_BG_BORDER_LITE), 0);
    lv_obj_set_style_border_width(badge, 1, 0);
    lv_obj_set_style_radius(badge, 8, 0);
    decor_no_input(badge);

    lv_obj_t *accent = lv_obj_create(parent);
    lv_obj_remove_style_all(accent);
    lv_obj_set_pos(accent, W7_HEADER_BADGE_X + 24, W7_HEADER_BADGE_Y + W7_HEADER_BADGE_H - 2);
    lv_obj_set_size(accent, W7_HEADER_BADGE_W - 48, 2);
    lv_obj_set_style_bg_color(accent, lv_color_hex(W7_BG_ACCENT_BLUE), 0);
    lv_obj_set_style_bg_opa(accent, LV_OPA_80, 0);
    decor_no_input(accent);

    make_screw(parent, W7_SCREW_TL_X, W7_SCREW_TL_Y);
    make_screw(parent, W7_SCREW_TR_X, W7_SCREW_TR_Y);
    make_screw(parent, W7_SCREW_BL_X, W7_SCREW_BL_Y);
    make_screw(parent, W7_SCREW_BR_X, W7_SCREW_BR_Y);
    make_screw(parent, W7_HEADER_BADGE_X + 12, W7_HEADER_BADGE_Y + (W7_HEADER_BADGE_H / 2));
    make_screw(parent, W7_HEADER_BADGE_X + W7_HEADER_BADGE_W - 12,
               W7_HEADER_BADGE_Y + (W7_HEADER_BADGE_H / 2));
}

static lv_obj_t *make_panel(lv_obj_t *parent, lv_coord_t x, lv_coord_t y, lv_coord_t w, lv_coord_t h)
{
    lv_obj_t *p = lv_obj_create(parent);
    cockpit_style_apply_panel(p);
    lv_obj_set_size(p, w, h);
    lv_obj_set_pos(p, x, y);
    lv_obj_remove_flag(p, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(p, lv_color_hex(0x0c1218), 0);
    lv_obj_set_style_border_color(p, lv_color_hex(0x3a4a5a), 0);
    lv_obj_set_style_border_width(p, 1, 0);
    lv_obj_set_style_radius(p, 6, 0);
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

static bool rects_overlap(const cockpit_wide_rect_t *a, const cockpit_wide_rect_t *b)
{
    return (a->x < b->x + b->w) && (a->x + a->w > b->x) && (a->y < b->y + b->h) &&
           (a->y + a->h > b->y);
}

static bool overlap_is_bad(const cockpit_wide_rect_t *a, const cockpit_wide_rect_t *b)
{
    if (a->role == W7_RECT_ROLE_DECORATIVE || b->role == W7_RECT_ROLE_DECORATIVE) {
        return false;
    }
    if (a->role == W7_RECT_ROLE_NAV || b->role == W7_RECT_ROLE_NAV) {
        return true;
    }
    if (a->role == W7_RECT_ROLE_CARD && b->role == W7_RECT_ROLE_CARD) {
        return true;
    }
    if (a->role == W7_RECT_ROLE_MAIN_GAUGE && b->role == W7_RECT_ROLE_MAIN_GAUGE) {
        return true;
    }
    if ((a->role == W7_RECT_ROLE_MAIN_GAUGE && b->role == W7_RECT_ROLE_CARD) ||
        (b->role == W7_RECT_ROLE_MAIN_GAUGE && a->role == W7_RECT_ROLE_CARD)) {
        return true;
    }
    if ((a->role == W7_RECT_ROLE_TUCKED_GAUGE && b->role == W7_RECT_ROLE_TUCKED_GAUGE)) {
        return true;
    }
    if ((a->role == W7_RECT_ROLE_TUCKED_GAUGE && b->role == W7_RECT_ROLE_CARD) ||
        (b->role == W7_RECT_ROLE_TUCKED_GAUGE && a->role == W7_RECT_ROLE_CARD)) {
        return false;
    }
    if (a->role == W7_RECT_ROLE_TOUCH || b->role == W7_RECT_ROLE_TOUCH) {
        return true;
    }
    return false;
}

#if FISHDUINO_COCKPIT_WIDE_LAYOUT_DEBUG
static void cockpit_wide_layout_check(const cockpit_wide_rect_t *rects, size_t count)
{
    for (size_t i = 0; i < count; i++) {
        const cockpit_wide_rect_t *r = &rects[i];
        if (r->x < 0 || r->y < 0 || r->x + r->w > W7_SCREEN_W || r->y + r->h > W7_SCREEN_H) {
            ESP_LOGW(TAG_WIDE, "bounds: '%s' [%d,%d %dx%d] outside %dx%d", r->name, r->x, r->y, r->w,
                     r->h, W7_SCREEN_W, W7_SCREEN_H);
        }
        if (r->role == W7_RECT_ROLE_TOUCH && r->h < W7_MIN_TOUCH_H) {
            ESP_LOGW(TAG_WIDE, "touch: '%s' height %d < %d", r->name, r->h, W7_MIN_TOUCH_H);
        }
        for (size_t j = i + 1; j < count; j++) {
            if (!rects_overlap(r, &rects[j])) {
                continue;
            }
            if (overlap_is_bad(r, &rects[j])) {
                ESP_LOGW(TAG_WIDE, "overlap: '%s' vs '%s'", r->name, rects[j].name);
            } else {
                ESP_LOGD(TAG_WIDE, "overlap ok: '%s' vs '%s'", r->name, rects[j].name);
            }
        }
    }
}
#endif

static void build_wide_cards(fishduino_cockpit_handles_t *out, lv_obj_t *parent)
{
    out->panel_water = make_panel(parent, W7_WATER_X, W7_CARD_ROW_Y, W7_WATER_W, W7_CARD_ROW_H);
    panel_title(out->panel_water, "WATER QUALITY");
    lv_obj_add_flag(out->panel_water, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(out->panel_water, tap_water_cb, LV_EVENT_CLICKED, NULL);
    make_water_column(out->panel_water, 8, "pH", &out->lbl_water_ph, &out->badge_water_ph);
    make_water_column(out->panel_water, 62, "NH3", &out->lbl_water_nh3, &out->badge_water_nh3);
    make_water_column(out->panel_water, 116, "NO2", &out->lbl_water_no2, &out->badge_water_no2);
    make_water_column(out->panel_water, 170, "NO3", &out->lbl_water_no3, &out->badge_water_no3);
    out->lbl_water_updated = lv_label_create(out->panel_water);
    lv_label_set_text(out->lbl_water_updated, "Updated --");
    lv_obj_set_style_text_color(out->lbl_water_updated, cockpit_color_dim(), 0);
    lv_obj_set_style_text_font(out->lbl_water_updated, cockpit_font_small(), 0);
    lv_obj_align(out->lbl_water_updated, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    out->btn_water_view = make_text_btn(out->panel_water, "VIEW ALL", tap_water_cb);
    lv_obj_set_size(out->btn_water_view, 80, W7_MIN_TOUCH_H);
    lv_obj_align(out->btn_water_view, LV_ALIGN_BOTTOM_RIGHT, 0, 0);

    out->panel_reminders = make_panel(parent, W7_REMINDERS_X, W7_CARD_ROW_Y, W7_REMINDERS_W, W7_CARD_ROW_H);
    panel_title(out->panel_reminders, "REMINDERS");
    lv_obj_add_flag(out->panel_reminders, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(out->panel_reminders, tap_reminders_cb, LV_EVENT_CLICKED, NULL);
    out->lbl_reminders_due = lv_label_create(out->panel_reminders);
    lv_label_set_text(out->lbl_reminders_due, "0 DUE");
    lv_obj_set_style_text_color(out->lbl_reminders_due, cockpit_color_amber(), 0);
    lv_obj_set_style_text_font(out->lbl_reminders_due, cockpit_font_value(), 0);
    lv_obj_align(out->lbl_reminders_due, LV_ALIGN_LEFT_MID, 0, -10);
    out->lbl_reminders_next = lv_label_create(out->panel_reminders);
    lv_label_set_text(out->lbl_reminders_next, "NEXT: --");
    lv_obj_set_style_text_color(out->lbl_reminders_next, cockpit_color_dim(), 0);
    lv_obj_set_style_text_font(out->lbl_reminders_next, cockpit_font_small(), 0);
    lv_obj_align(out->lbl_reminders_next, LV_ALIGN_BOTTOM_LEFT, 0, 8);
    out->btn_reminders_view = make_text_btn(out->panel_reminders, "VIEW", tap_reminders_cb);
    lv_obj_set_size(out->btn_reminders_view, 56, W7_MIN_TOUCH_H);
    lv_obj_align(out->btn_reminders_view, LV_ALIGN_BOTTOM_RIGHT, 0, 0);

    out->panel_clock = make_panel(parent, W7_CLOCK_X, W7_CARD_ROW_Y, W7_CLOCK_W, W7_CARD_ROW_H);
    panel_title(out->panel_clock, "CLOCK");
    out->lbl_clock_time = lv_label_create(out->panel_clock);
    lv_label_set_text(out->lbl_clock_time, "--:--");
    lv_obj_set_style_text_color(out->lbl_clock_time, cockpit_color_cyan(), 0);
    lv_obj_set_style_text_font(out->lbl_clock_time, cockpit_font_value(), 0);
    lv_obj_align(out->lbl_clock_time, LV_ALIGN_LEFT_MID, 0, -12);
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

    out->panel_systems = make_panel(parent, W7_SYSTEMS_X, W7_CARD_ROW_Y, W7_SYSTEMS_W, W7_CARD_ROW_H);
    panel_title(out->panel_systems, "SYSTEMS");
    struct {
        const char *name;
        lv_coord_t x;
        lv_coord_t y;
        lv_obj_t **led;
    } sys_leds[] = {
        {"Wi-Fi", 0, 18, &out->led_wifi},
        {"Shelly CO2", 0, 36, &out->led_co2},
        {"Filter", 0, 54, &out->led_filter},
        {"Heater", 0, 72, &out->led_heater},
        {"BLE", 108, 18, &out->led_ble},
        {"Feeder", 108, 36, &out->led_feeder},
        {"Light", 108, 54, &out->led_light},
        {"Alerts", 108, 72, &out->led_alerts},
    };
    for (size_t i = 0; i < sizeof(sys_leds) / sizeof(sys_leds[0]); i++) {
        lv_obj_t *lbl = lv_label_create(out->panel_systems);
        lv_label_set_text(lbl, sys_leds[i].name);
        lv_obj_set_style_text_color(lbl, cockpit_color_dim(), 0);
        lv_obj_set_style_text_font(lbl, cockpit_font_small(), 0);
        lv_obj_set_pos(lbl, sys_leds[i].x, sys_leds[i].y);
        lv_obj_t *led = lv_led_create(out->panel_systems);
        lv_obj_set_size(led, 12, 12);
        lv_obj_set_pos(led, sys_leds[i].x + 88, sys_leds[i].y + 2);
        lv_led_off(led);
        *sys_leds[i].led = led;
    }

    out->panel_light = make_panel(parent, W7_LIGHT_X, W7_CARD_ROW_Y, W7_LIGHT_W, W7_CARD_ROW_H);
    panel_title(out->panel_light, "LIGHTING");
    lv_obj_add_flag(out->panel_light, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(out->panel_light, tap_light_cb, LV_EVENT_CLICKED, NULL);
    out->lbl_light_mode = lv_label_create(out->panel_light);
    lv_label_set_text(out->lbl_light_mode, "DAYLIGHT");
    lv_obj_set_style_text_color(out->lbl_light_mode, cockpit_color_text(), 0);
    lv_obj_set_style_text_font(out->lbl_light_mode, cockpit_font_body(), 0);
    lv_obj_align(out->lbl_light_mode, LV_ALIGN_TOP_LEFT, 0, 18);
    out->bar_light = lv_bar_create(out->panel_light);
    lv_obj_set_size(out->bar_light, W7_LIGHT_W - 36, 12);
    lv_obj_align(out->bar_light, LV_ALIGN_LEFT_MID, 0, 6);
    lv_bar_set_range(out->bar_light, 0, 100);
    lv_obj_set_style_bg_color(out->bar_light, lv_color_hex(0x1a2838), LV_PART_MAIN);
    lv_obj_set_style_bg_color(out->bar_light, cockpit_color_amber(), LV_PART_INDICATOR);
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
}

void fishduino_cockpit_dashboard_build_wide(lv_obj_t *parent, fishduino_cockpit_handles_t *out)
{
    memset(out, 0, sizeof(*out));

    lv_obj_set_style_bg_color(parent, lv_color_hex(W7_BG_COLOR), 0);
    lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, 0);

    build_w7_background(parent);

    /* Alert banner (above background; below gauges when hidden) */
    out->banner = make_panel(parent, W7_MARGIN, W7_HEADER_Y, W7_INNER_W, W7_HEADER_H);
    lv_obj_set_style_bg_color(out->banner, lv_color_hex(0x331111), 0);
    lv_obj_add_flag(out->banner, LV_OBJ_FLAG_HIDDEN);
    out->lbl_banner = lv_label_create(out->banner);
    lv_label_set_text(out->lbl_banner, "");
    lv_obj_set_style_text_color(out->lbl_banner, cockpit_color_red(), 0);
    lv_obj_set_style_text_font(out->lbl_banner, cockpit_font_body(), 0);
    lv_obj_center(out->lbl_banner);

    /* Layer 2: main gauge cluster (hero temp center) */
    cockpit_gauge_create(&out->gauge_filter, parent, W7_GX_FILTER, W7_SIDE_GAUGE_Y, W7_FILTER_GAUGE_SZ,
                         "FILTER", "WATTS", GAUGE_START, GAUGE_END, 0.0f, 30.0f);
    cockpit_gauge_set_scale_ticks(&out->gauge_filter, "0", "30W");
    lv_obj_add_flag(out->gauge_filter.root, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(out->gauge_filter.root, tap_filter_cb, LV_EVENT_CLICKED, NULL);

    cockpit_gauge_create(&out->gauge_temp, parent, W7_GX_TEMP, W7_TEMP_GAUGE_Y, W7_TEMP_GAUGE_SZ, "TEMP",
                         "TANK F", GAUGE_START, GAUGE_END, 70.0f, 90.0f);
    cockpit_gauge_set_scale_ticks(&out->gauge_temp, "70", "90");
    lv_obj_add_flag(out->gauge_temp.root, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(out->gauge_temp.root, tap_heater_cb, LV_EVENT_CLICKED, NULL);

    out->lbl_temp_setpoint = lv_label_create(out->gauge_temp.ring_outer);
    lv_label_set_text(out->lbl_temp_setpoint, "SET --");
    lv_obj_set_style_text_color(out->lbl_temp_setpoint, cockpit_color_cyan(), 0);
    lv_obj_set_style_text_font(out->lbl_temp_setpoint, cockpit_font_small(), 0);
    lv_obj_align(out->lbl_temp_setpoint, LV_ALIGN_CENTER, 0, 48);

    out->lbl_heater_relay = lv_label_create(out->gauge_temp.root);
    lv_label_set_text(out->lbl_heater_relay, "RELAY --");
    lv_obj_set_style_text_color(out->lbl_heater_relay, cockpit_color_amber(), 0);
    lv_obj_set_style_text_font(out->lbl_heater_relay, cockpit_font_body(), 0);
    lv_obj_align(out->lbl_heater_relay, LV_ALIGN_BOTTOM_MID, 0, -40);

    out->lbl_heater_shelly = lv_label_create(out->gauge_temp.root);
    lv_label_set_text(out->lbl_heater_shelly, "SHELLY --");
    lv_obj_set_style_text_color(out->lbl_heater_shelly, cockpit_color_dim(), 0);
    lv_obj_set_style_text_font(out->lbl_heater_shelly, cockpit_font_small(), 0);
    lv_obj_align(out->lbl_heater_shelly, LV_ALIGN_BOTTOM_MID, 0, -56);

    cockpit_gauge_create(&out->gauge_co2, parent, W7_GX_CO2, W7_SIDE_GAUGE_Y, W7_CO2_GAUGE_SZ, "CO2",
                         "INJECTION", GAUGE_START, GAUGE_END, 0.0f, 1.0f);
    cockpit_gauge_set_scale_ticks(&out->gauge_co2, "OFF", "ON");
    lv_obj_add_flag(out->gauge_co2.root, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(out->gauge_co2.root, tap_co2_cb, LV_EVENT_CLICKED, NULL);

    /* Layer 3: tucked secondary gauges (may sit under card row visually) */
    cockpit_gauge_create(&out->gauge_feeder, parent, W7_FEEDER_GAUGE_X, W7_FEEDER_GAUGE_Y,
                         W7_FEEDER_GAUGE_SZ, "FEEDER", "SCHEDULE", GAUGE_START, GAUGE_END, 0.0f, 1.0f);
    lv_obj_add_flag(out->gauge_feeder.root, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(out->gauge_feeder.root, tap_feeder_cb, LV_EVENT_CLICKED, NULL);

    out->panel_trend =
        make_panel(parent, W7_TREND_PANEL_X, W7_TREND_PANEL_Y, W7_TREND_PANEL_W, W7_TREND_PANEL_H);
    panel_title(out->panel_trend, "TEMP 24H");
    out->lbl_trend_min = lv_label_create(out->panel_trend);
    lv_label_set_text(out->lbl_trend_min, "MIN --");
    lv_obj_set_style_text_color(out->lbl_trend_min, cockpit_color_cyan(), 0);
    lv_obj_set_style_text_font(out->lbl_trend_min, cockpit_font_small(), 0);
    lv_obj_align(out->lbl_trend_min, LV_ALIGN_TOP_RIGHT, 0, 0);
    out->lbl_trend_max = lv_label_create(out->panel_trend);
    lv_label_set_text(out->lbl_trend_max, "MAX --");
    lv_obj_set_style_text_color(out->lbl_trend_max, cockpit_color_red(), 0);
    lv_obj_set_style_text_font(out->lbl_trend_max, cockpit_font_small(), 0);
    lv_obj_align(out->lbl_trend_max, LV_ALIGN_TOP_RIGHT, 0, 14);

    out->chart_temp = lv_chart_create(out->panel_trend);
    lv_obj_set_size(out->chart_temp, W7_TREND_PANEL_W - 12, W7_TREND_PANEL_H - 30);
    lv_obj_align(out->chart_temp, LV_ALIGN_BOTTOM_MID, 0, -2);
    lv_obj_set_style_bg_color(out->chart_temp, lv_color_hex(0x080c10), 0);
    lv_obj_set_style_border_color(out->chart_temp, lv_color_hex(0x334455), 0);
    lv_obj_set_style_border_width(out->chart_temp, 1, 0);
    lv_chart_set_type(out->chart_temp, LV_CHART_TYPE_LINE);
    lv_chart_set_range(out->chart_temp, LV_CHART_AXIS_PRIMARY_Y, 0, 100);
    lv_chart_set_point_count(out->chart_temp, DASHBOARD_TEMP_HISTORY_LEN);
    out->chart_temp_series = lv_chart_add_series(out->chart_temp, cockpit_color_cyan(),
                                                 LV_CHART_AXIS_PRIMARY_Y);

    /* Layer 4: bottom status cards (drawn above tucked gauge bezels) */
    build_wide_cards(out, parent);

    /* Layer 5: navigation */
    out->nav_bar = lv_obj_create(parent);
    cockpit_style_apply_nav_bar(out->nav_bar);
    lv_obj_set_size(out->nav_bar, W7_INNER_W, W7_NAV_H);
    lv_obj_set_pos(out->nav_bar, W7_MARGIN, W7_NAV_Y);
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
    const lv_coord_t nav_btn_w = (W7_INNER_W - (NAV_COUNT - 1) * 8) / NAV_COUNT;
    for (int i = 0; i < NAV_COUNT; i++) {
        out->nav_btns[i] = lv_btn_create(out->nav_bar);
        lv_obj_set_size(out->nav_btns[i], nav_btn_w, W7_MIN_TOUCH_H);
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

    /* Brand on top for readability */
    out->lbl_brand = lv_label_create(parent);
    lv_label_set_text(out->lbl_brand, "AQUAPILOT");
    lv_obj_set_style_text_color(out->lbl_brand, cockpit_color_cyan(), 0);
    lv_obj_set_style_text_font(out->lbl_brand, cockpit_font_title(), 0);
    lv_obj_align(out->lbl_brand, LV_ALIGN_TOP_MID, 0, W7_BRAND_Y);
    lv_obj_move_foreground(out->lbl_brand);
    lv_obj_move_foreground(out->banner);

#if FISHDUINO_COCKPIT_WIDE_LAYOUT_DEBUG
    const cockpit_wide_rect_t layout_rects[] = {
        {"banner", W7_MARGIN, W7_HEADER_Y, W7_INNER_W, W7_HEADER_H, W7_RECT_ROLE_DECORATIVE},
        {"gauge_filter", W7_GX_FILTER, W7_SIDE_GAUGE_Y, W7_FILTER_GAUGE_SZ,
         W7_GAUGE_ROOT_H(W7_FILTER_GAUGE_SZ), W7_RECT_ROLE_MAIN_GAUGE},
        {"gauge_temp", W7_GX_TEMP, W7_TEMP_GAUGE_Y, W7_TEMP_GAUGE_SZ, W7_GAUGE_ROOT_H(W7_TEMP_GAUGE_SZ),
         W7_RECT_ROLE_MAIN_GAUGE},
        {"gauge_co2", W7_GX_CO2, W7_SIDE_GAUGE_Y, W7_CO2_GAUGE_SZ, W7_GAUGE_ROOT_H(W7_CO2_GAUGE_SZ),
         W7_RECT_ROLE_MAIN_GAUGE},
        {"gauge_feeder", W7_FEEDER_GAUGE_X, W7_FEEDER_GAUGE_Y, W7_FEEDER_GAUGE_SZ,
         W7_GAUGE_ROOT_H(W7_FEEDER_GAUGE_SZ), W7_RECT_ROLE_TUCKED_GAUGE},
        {"panel_trend", W7_TREND_PANEL_X, W7_TREND_PANEL_Y, W7_TREND_PANEL_W, W7_TREND_PANEL_H,
         W7_RECT_ROLE_TUCKED_GAUGE},
        {"panel_water", W7_WATER_X, W7_CARD_ROW_Y, W7_WATER_W, W7_CARD_ROW_H, W7_RECT_ROLE_CARD},
        {"panel_reminders", W7_REMINDERS_X, W7_CARD_ROW_Y, W7_REMINDERS_W, W7_CARD_ROW_H,
         W7_RECT_ROLE_CARD},
        {"panel_clock", W7_CLOCK_X, W7_CARD_ROW_Y, W7_CLOCK_W, W7_CARD_ROW_H, W7_RECT_ROLE_CARD},
        {"panel_systems", W7_SYSTEMS_X, W7_CARD_ROW_Y, W7_SYSTEMS_W, W7_CARD_ROW_H, W7_RECT_ROLE_CARD},
        {"panel_light", W7_LIGHT_X, W7_CARD_ROW_Y, W7_LIGHT_W, W7_CARD_ROW_H, W7_RECT_ROLE_CARD},
        {"nav_bar", W7_MARGIN, W7_NAV_Y, W7_INNER_W, W7_NAV_H, W7_RECT_ROLE_NAV},
    };
    cockpit_wide_layout_check(layout_rects, sizeof(layout_rects) / sizeof(layout_rects[0]));
#endif
}

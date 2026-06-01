#include "cockpit_gauge.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define CKPT_BG        0x0a0e14
#define CKPT_PANEL     0x141c24
#define CKPT_EDGE      0x334455
#define CKPT_CYAN      0x00d4e8
#define CKPT_GREEN     0x00e676
#define CKPT_RED       0xff3d57
#define CKPT_AMBER     0xffb020
#define CKPT_TEXT      0xe8eef4
#define CKPT_DIM       0x8899aa
#define CKPT_METAL     0x2a3848

lv_color_t cockpit_color_green(void)
{
    return lv_color_hex(CKPT_GREEN);
}
lv_color_t cockpit_color_red(void)
{
    return lv_color_hex(CKPT_RED);
}
lv_color_t cockpit_color_amber(void)
{
    return lv_color_hex(CKPT_AMBER);
}
lv_color_t cockpit_color_cyan(void)
{
    return lv_color_hex(CKPT_CYAN);
}
lv_color_t cockpit_color_dim(void)
{
    return lv_color_hex(CKPT_DIM);
}
lv_color_t cockpit_color_text(void)
{
    return lv_color_hex(CKPT_TEXT);
}

const lv_font_t *cockpit_font_title(void)
{
#if LV_FONT_MONTSERRAT_16
    return &lv_font_montserrat_16;
#else
    return &lv_font_montserrat_14;
#endif
}

const lv_font_t *cockpit_font_value(void)
{
#if LV_FONT_MONTSERRAT_24
    return &lv_font_montserrat_24;
#elif LV_FONT_MONTSERRAT_20
    return &lv_font_montserrat_20;
#else
    return &lv_font_montserrat_14;
#endif
}

const lv_font_t *cockpit_font_body(void)
{
#if LV_FONT_MONTSERRAT_16
    return &lv_font_montserrat_16;
#else
    return &lv_font_montserrat_14;
#endif
}

const lv_font_t *cockpit_font_small(void)
{
    return &lv_font_montserrat_14;
}

void cockpit_style_apply_nav_bar(lv_obj_t *obj)
{
    lv_obj_remove_style_all(obj);
    lv_obj_set_style_bg_color(obj, lv_color_hex(0x0c1218), 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(obj, lv_color_hex(CKPT_EDGE), 0);
    lv_obj_set_style_border_width(obj, 1, 0);
    lv_obj_set_style_border_side(obj, LV_BORDER_SIDE_TOP, 0);
    lv_obj_set_style_pad_all(obj, 4, 0);
}

void cockpit_style_apply_panel(lv_obj_t *obj)
{
    lv_obj_set_style_bg_color(obj, lv_color_hex(CKPT_PANEL), 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(obj, lv_color_hex(CKPT_EDGE), 0);
    lv_obj_set_style_border_width(obj, 2, 0);
    lv_obj_set_style_radius(obj, 8, 0);
    lv_obj_set_style_shadow_width(obj, 8, 0);
    lv_obj_set_style_shadow_color(obj, lv_color_hex(0x000000), 0);
    lv_obj_set_style_shadow_opa(obj, LV_OPA_40, 0);
    lv_obj_set_style_pad_all(obj, 6, 0);
}

void cockpit_style_apply_instrument_ring(lv_obj_t *obj)
{
    lv_obj_remove_style_all(obj);
    lv_obj_set_style_bg_color(obj, lv_color_hex(CKPT_METAL), 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(obj, lv_color_hex(CKPT_EDGE), 0);
    lv_obj_set_style_border_width(obj, 3, 0);
    lv_obj_set_style_radius(obj, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_shadow_width(obj, 12, 0);
    lv_obj_set_style_shadow_color(obj, lv_color_hex(0x000000), 0);
    lv_obj_set_style_shadow_opa(obj, LV_OPA_50, 0);
}

static int32_t value_to_angle(const cockpit_gauge_t *g, float value)
{
    float span = g->max_val - g->min_val;
    if (span <= 0.0f) {
        return g->angle_start;
    }
    float ratio = (value - g->min_val) / span;
    if (ratio < 0.0f) {
        ratio = 0.0f;
    }
    if (ratio > 1.0f) {
        ratio = 1.0f;
    }
    return g->angle_start + (int32_t)(ratio * (float)(g->angle_end - g->angle_start));
}

static void style_arc_zone(lv_obj_t *arc, lv_color_t color, lv_coord_t width)
{
    lv_obj_remove_style(arc, NULL, LV_PART_KNOB);
    lv_obj_remove_flag(arc, LV_OBJ_FLAG_CLICKABLE);
    lv_arc_set_rotation(arc, 0);
    lv_obj_set_style_arc_color(arc, color, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(arc, width, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(arc, lv_color_hex(CKPT_METAL), LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc, width, LV_PART_MAIN);
    lv_obj_set_style_arc_opa(arc, LV_OPA_20, LV_PART_MAIN);
}

static void set_arc_span(lv_obj_t *arc, int32_t start, int32_t end)
{
    if (end <= start) {
        lv_obj_add_flag(arc, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    lv_obj_remove_flag(arc, LV_OBJ_FLAG_HIDDEN);
    lv_arc_set_bg_angles(arc, start, end);
    lv_arc_set_angles(arc, start, end);
}

static lv_obj_t *make_zone_arc(lv_obj_t *parent, lv_coord_t size)
{
    lv_obj_t *arc = lv_arc_create(parent);
    lv_obj_set_size(arc, size, size);
    lv_obj_center(arc);
    lv_arc_set_mode(arc, LV_ARC_MODE_NORMAL);
    lv_arc_set_value(arc, 100);
    return arc;
}

void cockpit_gauge_create(cockpit_gauge_t *g, lv_obj_t *parent, lv_coord_t x, lv_coord_t y, lv_coord_t size,
                          const char *title, const char *subtitle, int32_t start_angle, int32_t end_angle,
                          float min_val, float max_val)
{
    memset(g, 0, sizeof(*g));
    g->angle_start = start_angle;
    g->angle_end = end_angle;
    g->min_val = min_val;
    g->max_val = max_val;
    g->last_needle_angle = start_angle;

    g->root = lv_obj_create(parent);
    lv_obj_remove_style_all(g->root);
    lv_obj_set_size(g->root, size, size + 40);
    lv_obj_set_pos(g->root, x, y);
    lv_obj_remove_flag(g->root, LV_OBJ_FLAG_SCROLLABLE);

    g->ring_outer = lv_obj_create(g->root);
    cockpit_style_apply_instrument_ring(g->ring_outer);
    lv_obj_set_size(g->ring_outer, size, size);
    lv_obj_align(g->ring_outer, LV_ALIGN_TOP_MID, 0, 18);

    lv_coord_t arc_size = size - 12;
    g->arc_zone_low = make_zone_arc(g->ring_outer, arc_size);
    style_arc_zone(g->arc_zone_low, cockpit_color_red(), size > 150 ? 14 : 10);

    g->arc_zone_ok = make_zone_arc(g->ring_outer, arc_size - 4);
    style_arc_zone(g->arc_zone_ok, cockpit_color_green(), size > 150 ? 12 : 8);

    g->arc_zone_high = make_zone_arc(g->ring_outer, arc_size - 8);
    style_arc_zone(g->arc_zone_high, cockpit_color_red(), size > 150 ? 10 : 6);

    g->needle_pivot = lv_obj_create(g->ring_outer);
    lv_obj_remove_style_all(g->needle_pivot);
    lv_obj_set_size(g->needle_pivot, arc_size, arc_size);
    lv_obj_center(g->needle_pivot);
    lv_obj_remove_flag(g->needle_pivot, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_transform_pivot_x(g->needle_pivot, arc_size / 2, 0);
    lv_obj_set_style_transform_pivot_y(g->needle_pivot, arc_size / 2, 0);

    g->needle = lv_obj_create(g->needle_pivot);
    lv_obj_remove_style_all(g->needle);
    lv_coord_t needle_len = arc_size / 2 - 14;
    lv_obj_set_size(g->needle, size > 150 ? 5 : 3, needle_len);
    lv_obj_set_style_bg_color(g->needle, lv_color_hex(CKPT_CYAN), 0);
    lv_obj_set_style_bg_opa(g->needle, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(g->needle, 2, 0);
    lv_obj_align(g->needle, LV_ALIGN_CENTER, 0, -(needle_len / 2 + 2));

    g->hub = lv_obj_create(g->ring_outer);
    lv_obj_remove_style_all(g->hub);
    lv_coord_t hub = size > 150 ? 16 : 10;
    lv_obj_set_size(g->hub, hub, hub);
    lv_obj_set_style_bg_color(g->hub, lv_color_hex(CKPT_CYAN), 0);
    lv_obj_set_style_bg_opa(g->hub, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(g->hub, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_color(g->hub, lv_color_hex(CKPT_TEXT), 0);
    lv_obj_set_style_border_width(g->hub, 2, 0);
    lv_obj_center(g->hub);

    g->lbl_title = lv_label_create(g->root);
    lv_label_set_text(g->lbl_title, title ? title : "");
    lv_obj_set_style_text_color(g->lbl_title, cockpit_color_cyan(), 0);
    lv_obj_set_style_text_font(g->lbl_title, cockpit_font_title(), 0);
    lv_obj_align(g->lbl_title, LV_ALIGN_TOP_MID, 0, 0);

    g->lbl_subtitle = lv_label_create(g->root);
    lv_label_set_text(g->lbl_subtitle, subtitle ? subtitle : "");
    lv_obj_set_style_text_color(g->lbl_subtitle, cockpit_color_dim(), 0);
    lv_obj_set_style_text_font(g->lbl_subtitle, cockpit_font_small(), 0);
    lv_obj_align(g->lbl_subtitle, LV_ALIGN_TOP_MID, 0, 14);

    g->lbl_scale_lo = lv_label_create(g->ring_outer);
    lv_label_set_text(g->lbl_scale_lo, "");
    lv_obj_set_style_text_color(g->lbl_scale_lo, cockpit_color_dim(), 0);
    lv_obj_set_style_text_font(g->lbl_scale_lo, cockpit_font_small(), 0);
    lv_obj_align(g->lbl_scale_lo, LV_ALIGN_BOTTOM_LEFT, 8, -6);

    g->lbl_scale_hi = lv_label_create(g->ring_outer);
    lv_label_set_text(g->lbl_scale_hi, "");
    lv_obj_set_style_text_color(g->lbl_scale_hi, cockpit_color_dim(), 0);
    lv_obj_set_style_text_font(g->lbl_scale_hi, cockpit_font_small(), 0);
    lv_obj_align(g->lbl_scale_hi, LV_ALIGN_BOTTOM_RIGHT, -8, -6);

    g->lbl_value = lv_label_create(g->ring_outer);
    lv_label_set_text(g->lbl_value, "--");
    lv_obj_set_style_text_color(g->lbl_value, cockpit_color_text(), 0);
    lv_obj_set_style_text_font(g->lbl_value, cockpit_font_value(), 0);
    lv_obj_align(g->lbl_value, LV_ALIGN_CENTER, 0, size > 150 ? 6 : 2);

    g->lbl_sub = lv_label_create(g->root);
    lv_label_set_text(g->lbl_sub, "");
    lv_obj_set_style_text_color(g->lbl_sub, cockpit_color_dim(), 0);
    lv_obj_set_style_text_font(g->lbl_sub, cockpit_font_small(), 0);
    lv_obj_align(g->lbl_sub, LV_ALIGN_BOTTOM_MID, 0, -18);

    g->lbl_status = lv_label_create(g->root);
    lv_label_set_text(g->lbl_status, "");
    lv_obj_set_style_text_color(g->lbl_status, cockpit_color_green(), 0);
    lv_obj_set_style_text_font(g->lbl_status, cockpit_font_body(), 0);
    lv_obj_align(g->lbl_status, LV_ALIGN_BOTTOM_MID, 0, -2);
}

void cockpit_gauge_set_titles(cockpit_gauge_t *g, const char *title, const char *subtitle)
{
    if (g == NULL) {
        return;
    }
    if (g->lbl_title && title) {
        lv_label_set_text(g->lbl_title, title);
    }
    if (g->lbl_subtitle && subtitle) {
        lv_label_set_text(g->lbl_subtitle, subtitle);
    }
}

void cockpit_gauge_set_scale_ticks(cockpit_gauge_t *g, const char *lo, const char *hi)
{
    if (g == NULL) {
        return;
    }
    if (g->lbl_scale_lo && lo) {
        lv_label_set_text(g->lbl_scale_lo, lo);
    }
    if (g->lbl_scale_hi && hi) {
        lv_label_set_text(g->lbl_scale_hi, hi);
    }
}

void cockpit_gauge_set_needle(cockpit_gauge_t *g, float value, bool animate)
{
    int32_t angle = value_to_angle(g, value);
    g->last_needle_angle = angle;
    int32_t rot = (angle - 270) * 10;
    if (animate) {
        lv_obj_set_style_transform_rotation(g->needle_pivot, rot, LV_PART_MAIN);
    } else {
        lv_obj_set_style_transform_rotation(g->needle_pivot, rot, LV_PART_MAIN);
    }
}

void cockpit_gauge_set_zones_linear(cockpit_gauge_t *g, float ok_lo, float ok_hi)
{
    int32_t a_lo = value_to_angle(g, ok_lo);
    int32_t a_hi = value_to_angle(g, ok_hi);
    set_arc_span(g->arc_zone_low, g->angle_start, a_lo);
    set_arc_span(g->arc_zone_ok, a_lo, a_hi);
    set_arc_span(g->arc_zone_high, a_hi, g->angle_end);
}

void cockpit_gauge_set_zones_on_off(cockpit_gauge_t *g, bool on)
{
    int32_t mid = g->angle_start + (g->angle_end - g->angle_start) / 2;
    if (on) {
        set_arc_span(g->arc_zone_low, g->angle_start, g->angle_start);
        set_arc_span(g->arc_zone_ok, g->angle_start, g->angle_end);
        lv_obj_set_style_arc_color(g->arc_zone_ok, cockpit_color_green(), LV_PART_INDICATOR);
        set_arc_span(g->arc_zone_high, g->angle_end, g->angle_end);
    } else {
        set_arc_span(g->arc_zone_low, g->angle_start, g->angle_end);
        lv_obj_set_style_arc_color(g->arc_zone_low, cockpit_color_red(), LV_PART_INDICATOR);
        set_arc_span(g->arc_zone_ok, g->angle_start, g->angle_start);
        set_arc_span(g->arc_zone_high, g->angle_end, g->angle_end);
    }
    (void)mid;
}

void cockpit_gauge_set_labels(cockpit_gauge_t *g, const char *value, const char *sub, const char *status)
{
    if (g->lbl_value && value) {
        lv_label_set_text(g->lbl_value, value);
    }
    if (g->lbl_sub && sub) {
        lv_label_set_text(g->lbl_sub, sub);
    }
    if (g->lbl_status && status) {
        lv_label_set_text(g->lbl_status, status);
    }
}

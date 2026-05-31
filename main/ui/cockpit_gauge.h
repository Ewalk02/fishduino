#pragma once

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    lv_obj_t *root;
    lv_obj_t *ring_outer;
    lv_obj_t *arc_zone_low;
    lv_obj_t *arc_zone_ok;
    lv_obj_t *arc_zone_high;
    lv_obj_t *needle_pivot;
    lv_obj_t *needle;
    lv_obj_t *hub;
    lv_obj_t *lbl_title;
    lv_obj_t *lbl_subtitle;
    lv_obj_t *lbl_value;
    lv_obj_t *lbl_sub;
    lv_obj_t *lbl_status;
    lv_obj_t *lbl_scale_lo;
    lv_obj_t *lbl_scale_hi;
    int32_t angle_start;
    int32_t angle_end;
    float min_val;
    float max_val;
    int32_t last_needle_angle;
} cockpit_gauge_t;

void cockpit_style_apply_panel(lv_obj_t *obj);
void cockpit_style_apply_instrument_ring(lv_obj_t *obj);
void cockpit_style_apply_nav_bar(lv_obj_t *obj);

const lv_font_t *cockpit_font_title(void);
const lv_font_t *cockpit_font_value(void);
const lv_font_t *cockpit_font_body(void);
const lv_font_t *cockpit_font_small(void);

void cockpit_gauge_create(cockpit_gauge_t *g, lv_obj_t *parent, lv_coord_t x, lv_coord_t y, lv_coord_t size,
                          const char *title, const char *subtitle, int32_t start_angle, int32_t end_angle,
                          float min_val, float max_val);

void cockpit_gauge_set_titles(cockpit_gauge_t *g, const char *title, const char *subtitle);
void cockpit_gauge_set_scale_ticks(cockpit_gauge_t *g, const char *lo, const char *hi);
void cockpit_gauge_set_needle(cockpit_gauge_t *g, float value, bool animate);
void cockpit_gauge_set_zones_linear(cockpit_gauge_t *g, float ok_lo, float ok_hi);
void cockpit_gauge_set_zones_on_off(cockpit_gauge_t *g, bool on);
void cockpit_gauge_set_labels(cockpit_gauge_t *g, const char *value, const char *sub, const char *status);

lv_color_t cockpit_color_green(void);
lv_color_t cockpit_color_red(void);
lv_color_t cockpit_color_amber(void);
lv_color_t cockpit_color_cyan(void);
lv_color_t cockpit_color_dim(void);
lv_color_t cockpit_color_text(void);

#ifdef __cplusplus
}
#endif

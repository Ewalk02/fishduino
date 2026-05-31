#include "screen_water_history.h"

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "water/water_metrics.h"

static lv_obj_t *s_screen;
static lv_obj_t *s_chart;
static lv_obj_t *s_label_list;
static lv_chart_series_t *s_series;

typedef enum {
    PARAM_PH = 0,
    PARAM_AMMONIA,
    PARAM_NITRITE,
    PARAM_NITRATE,
} param_t;

typedef enum {
    RANGE_30D = 30,
    RANGE_90D = 90,
    RANGE_ALL = 0,
} range_days_t;

static param_t s_param = PARAM_PH;
static range_days_t s_range = RANGE_30D;

static float get_param_value(const water_test_entry_t *e, param_t p)
{
    switch (p) {
    case PARAM_PH:
        return (e->valid_flags & WATER_VALID_PH) ? e->ph : NAN;
    case PARAM_AMMONIA:
        return (e->valid_flags & WATER_VALID_AMMONIA) ? e->ammonia_ppm : NAN;
    case PARAM_NITRITE:
        return (e->valid_flags & WATER_VALID_NITRITE) ? e->nitrite_ppm : NAN;
    default:
        return (e->valid_flags & WATER_VALID_NITRATE) ? e->nitrate_ppm : NAN;
    }
}

static bool in_range(int64_t ts, range_days_t range, int64_t now)
{
    if (range == RANGE_ALL || ts <= 0 || now <= 0) {
        return true;
    }
    int64_t cutoff = now - (int64_t)range * 86400LL;
    return ts >= cutoff;
}

static void refresh_view(void)
{
    if (s_chart == NULL || s_label_list == NULL) {
        return;
    }

    water_test_entry_t entries[WATER_METRICS_MAX_ENTRIES];
    size_t count = 0;
    water_metrics_get_entries(entries, WATER_METRICS_MAX_ENTRIES, &count);

    int64_t now = 0;
    if (count > 0 && entries[count - 1].timestamp_unix > 0) {
        now = entries[count - 1].timestamp_unix;
    }

    float values[64];
    int nvals = 0;
    char list_buf[512] = {0};

    for (size_t i = 0; i < count && nvals < 64; i++) {
        const water_test_entry_t *e = &entries[i];
        if (!in_range(e->timestamp_unix, s_range, now)) {
            continue;
        }
        float v = get_param_value(e, s_param);
        if (v != v) {
            continue;
        }
        values[nvals++] = v;

        char date[16];
        if (e->timestamp_unix > 0) {
            struct tm tm_local;
            time_t t = (time_t)e->timestamp_unix;
            if (localtime_r(&t, &tm_local) != NULL) {
                strftime(date, sizeof(date), "%m/%d", &tm_local);
            } else {
                snprintf(date, sizeof(date), "?");
            }
        } else {
            snprintf(date, sizeof(date), "?");
        }
        char line[48];
        snprintf(line, sizeof(line), "%s %.2f\n", date, (double)v);
        strncat(list_buf, line, sizeof(list_buf) - strlen(list_buf) - 1);
    }

    lv_chart_set_point_count(s_chart, nvals > 0 ? (uint16_t)nvals : 1);
    if (s_series != NULL) {
        for (int i = 0; i < nvals; i++) {
            lv_chart_set_value_by_id(s_chart, s_series, (uint16_t)i, (int32_t)(values[i] * 10.0f));
        }
    }
    lv_chart_refresh(s_chart);

    if (list_buf[0] == '\0') {
        lv_label_set_text(s_label_list, "No data in range");
    } else {
        lv_label_set_text(s_label_list, list_buf);
    }
}

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

static void set_param_cb(lv_event_t *e)
{
    s_param = (param_t)(intptr_t)lv_event_get_user_data(e);
    refresh_view();
}

static void set_range_cb(lv_event_t *e)
{
    s_range = (range_days_t)(intptr_t)lv_event_get_user_data(e);
    refresh_view();
}

void fishduino_screen_water_history_build(lv_obj_t *parent)
{
    s_screen = lv_obj_create(parent);
    lv_obj_set_size(s_screen, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(s_screen, lv_color_black(), 0);
    lv_obj_add_flag(s_screen, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t *title = lv_label_create(s_screen);
    lv_label_set_text(title, "Water History");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 4);

    const struct {
        const char *name;
        param_t p;
    } params[] = {{"pH", PARAM_PH}, {"NH3", PARAM_AMMONIA}, {"NO2", PARAM_NITRITE}, {"NO3", PARAM_NITRATE}};
    for (int i = 0; i < 4; i++) {
        lv_obj_t *btn = lv_btn_create(s_screen);
        lv_obj_set_size(btn, 70, 28);
        lv_obj_align(btn, LV_ALIGN_TOP_LEFT, 8 + i * 76, 28);
        lv_label_set_text(lv_label_create(btn), params[i].name);
        lv_obj_add_event_cb(btn, set_param_cb, LV_EVENT_CLICKED, (void *)(intptr_t)params[i].p);
    }

    const struct {
        const char *name;
        range_days_t r;
    } ranges[] = {{"30d", RANGE_30D}, {"90d", RANGE_90D}, {"All", RANGE_ALL}};
    for (int i = 0; i < 3; i++) {
        lv_obj_t *btn = lv_btn_create(s_screen);
        lv_obj_set_size(btn, 60, 28);
        lv_obj_align(btn, LV_ALIGN_TOP_LEFT, 8 + i * 66, 60);
        lv_label_set_text(lv_label_create(btn), ranges[i].name);
        lv_obj_add_event_cb(btn, set_range_cb, LV_EVENT_CLICKED, (void *)(intptr_t)ranges[i].r);
    }

    s_chart = lv_chart_create(s_screen);
    lv_obj_set_size(s_chart, 420, 120);
    lv_obj_align(s_chart, LV_ALIGN_TOP_MID, 0, 96);
    lv_chart_set_type(s_chart, LV_CHART_TYPE_LINE);
    lv_chart_set_range(s_chart, LV_CHART_AXIS_PRIMARY_Y, 0, 100);
    s_series = lv_chart_add_series(s_chart, lv_palette_main(LV_PALETTE_CYAN), LV_CHART_AXIS_PRIMARY_Y);

    s_label_list = lv_label_create(s_screen);
    lv_obj_set_width(s_label_list, 420);
    lv_label_set_long_mode(s_label_list, LV_LABEL_LONG_WRAP);
    lv_obj_align(s_label_list, LV_ALIGN_TOP_LEFT, 12, 224);

    lv_obj_t *btn_back = lv_btn_create(s_screen);
    lv_obj_set_size(btn_back, 100, 36);
    lv_obj_align(btn_back, LV_ALIGN_BOTTOM_MID, 0, -8);
    lv_label_set_text(lv_label_create(btn_back), "BACK");
    lv_obj_add_event_cb(btn_back, btn_back_cb, LV_EVENT_CLICKED, NULL);
}

void fishduino_screen_water_history_show(void)
{
    if (s_screen == NULL) {
        return;
    }
    refresh_view();
    lv_obj_clear_flag(s_screen, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_screen);
}

void fishduino_screen_water_history_hide(void)
{
    hide_self();
}

lv_obj_t *fishduino_screen_water_history_root(void)
{
    return s_screen;
}

#include "screen_co2_schedule.h"

#include <stdio.h>
#include <string.h>

#include "storage/settings_runtime.h"

static lv_obj_t *s_screen;
static lv_obj_t *s_label_status;
static uint16_t s_on_h, s_on_m, s_off_h, s_off_m;

typedef struct {
    uint16_t on_min;
    uint16_t off_min;
} co2_times_ctx_t;

static bool mutator_co2_times(fishduino_settings_t *st, void *ctx)
{
    co2_times_ctx_t *t = (co2_times_ctx_t *)ctx;
    if (t->on_min == t->off_min) {
        return false;
    }
    st->co2.on_min = t->on_min;
    st->co2.off_min = t->off_min;
    return true;
}

static void refresh_preview(void)
{
    char buf[96];
    snprintf(buf, sizeof(buf), "ON %02u:%02u  OFF %02u:%02u", (unsigned)s_on_h, (unsigned)s_on_m,
             (unsigned)s_off_h, (unsigned)s_off_m);
    if (s_on_h * 60 + s_on_m > s_off_h * 60 + s_off_m) {
        strncat(buf, " (crosses midnight)", sizeof(buf) - strlen(buf) - 1);
    }
    lv_label_set_text(s_label_status, buf);
}

static void clamp_hm(uint16_t *h, uint16_t *m)
{
    if (*h >= 24) {
        *h = 23;
    }
    if (*m >= 60) {
        *m = 59;
    }
}

static void btn_back_cb(lv_event_t *e)
{
    (void)e;
    fishduino_screen_co2_schedule_hide();
}

static void btn_save_cb(lv_event_t *e)
{
    (void)e;
    co2_times_ctx_t t = {
        .on_min = (uint16_t)(s_on_h * 60 + s_on_m),
        .off_min = (uint16_t)(s_off_h * 60 + s_off_m),
    };
    if (t.on_min == t.off_min) {
        lv_label_set_text(s_label_status, "ON and OFF times must differ");
        return;
    }
    if (fishduino_settings_update(mutator_co2_times, &t, true)) {
        lv_label_set_text(s_label_status, "Schedule saved to NVS");
    } else {
        lv_label_set_text(s_label_status, "Save failed");
    }
}

static void make_stepper(lv_obj_t *parent, int y, const char *name, void (*dec)(lv_event_t *),
                         void (*inc)(lv_event_t *))
{
    lv_obj_t *lbl = lv_label_create(parent);
    lv_label_set_text(lbl, name);
    lv_obj_align(lbl, LV_ALIGN_TOP_LEFT, 8, y);

    lv_obj_t *bd = lv_btn_create(parent);
    lv_obj_set_size(bd, 40, 32);
    lv_obj_align(bd, LV_ALIGN_TOP_LEFT, 100, y - 4);
    lv_label_set_text(lv_label_create(bd), "-");
    lv_obj_add_event_cb(bd, dec, LV_EVENT_CLICKED, NULL);

    lv_obj_t *bi = lv_btn_create(parent);
    lv_obj_set_size(bi, 40, 32);
    lv_obj_align(bi, LV_ALIGN_TOP_LEFT, 150, y - 4);
    lv_label_set_text(lv_label_create(bi), "+");
    lv_obj_add_event_cb(bi, inc, LV_EVENT_CLICKED, NULL);
}

static void on_h_dec(lv_event_t *e)
{
    (void)e;
    if (s_on_h > 0) {
        s_on_h--;
    }
    refresh_preview();
}
static void on_h_inc(lv_event_t *e)
{
    (void)e;
    s_on_h = (s_on_h + 1) % 24;
    refresh_preview();
}
static void on_m_dec(lv_event_t *e)
{
    (void)e;
    if (s_on_m > 0) {
        s_on_m--;
    } else if (s_on_h > 0) {
        s_on_h--;
        s_on_m = 59;
    }
    clamp_hm(&s_on_h, &s_on_m);
    refresh_preview();
}
static void on_m_inc(lv_event_t *e)
{
    (void)e;
    s_on_m++;
    if (s_on_m >= 60) {
        s_on_m = 0;
        s_on_h = (s_on_h + 1) % 24;
    }
    refresh_preview();
}
static void off_h_dec(lv_event_t *e)
{
    (void)e;
    if (s_off_h > 0) {
        s_off_h--;
    }
    refresh_preview();
}
static void off_h_inc(lv_event_t *e)
{
    (void)e;
    s_off_h = (s_off_h + 1) % 24;
    refresh_preview();
}
static void off_m_dec(lv_event_t *e)
{
    (void)e;
    if (s_off_m > 0) {
        s_off_m--;
    } else if (s_off_h > 0) {
        s_off_h--;
        s_off_m = 59;
    }
    clamp_hm(&s_off_h, &s_off_m);
    refresh_preview();
}
static void off_m_inc(lv_event_t *e)
{
    (void)e;
    s_off_m++;
    if (s_off_m >= 60) {
        s_off_m = 0;
        s_off_h = (s_off_h + 1) % 24;
    }
    refresh_preview();
}

void fishduino_screen_co2_schedule_build(lv_obj_t *parent)
{
    s_screen = lv_obj_create(parent);
    lv_obj_set_size(s_screen, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(s_screen, lv_color_black(), 0);
    lv_obj_add_flag(s_screen, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t *title = lv_label_create(s_screen);
    lv_label_set_text(title, "CO2 Schedule Times");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 8);

    s_label_status = lv_label_create(s_screen);
    lv_obj_set_width(s_label_status, 440);
    lv_obj_align(s_label_status, LV_ALIGN_TOP_MID, 0, 32);

    make_stepper(s_screen, 64, "ON hour", on_h_dec, on_h_inc);
    make_stepper(s_screen, 104, "ON min", on_m_dec, on_m_inc);
    make_stepper(s_screen, 144, "OFF hour", off_h_dec, off_h_inc);
    make_stepper(s_screen, 184, "OFF min", off_m_dec, off_m_inc);

    lv_obj_t *btn_save = lv_btn_create(s_screen);
    lv_obj_set_size(btn_save, 160, 40);
    lv_obj_align(btn_save, LV_ALIGN_TOP_MID, 0, 230);
    lv_label_set_text(lv_label_create(btn_save), "SAVE");
    lv_obj_add_event_cb(btn_save, btn_save_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *btn_back = lv_btn_create(s_screen);
    lv_obj_set_size(btn_back, 100, 36);
    lv_obj_align(btn_back, LV_ALIGN_BOTTOM_MID, 0, -8);
    lv_label_set_text(lv_label_create(btn_back), "BACK");
    lv_obj_add_event_cb(btn_back, btn_back_cb, LV_EVENT_CLICKED, NULL);
}

void fishduino_screen_co2_schedule_show(void)
{
    if (s_screen == NULL) {
        return;
    }

    fishduino_settings_t st;
    if (fishduino_settings_get_snapshot(&st)) {
        s_on_h = st.co2.on_min / 60;
        s_on_m = st.co2.on_min % 60;
        s_off_h = st.co2.off_min / 60;
        s_off_m = st.co2.off_min % 60;
    }
    refresh_preview();
    lv_obj_clear_flag(s_screen, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_screen);
}

void fishduino_screen_co2_schedule_hide(void)
{
    if (s_screen != NULL) {
        lv_obj_add_flag(s_screen, LV_OBJ_FLAG_HIDDEN);
    }
}

lv_obj_t *fishduino_screen_co2_schedule_root(void)
{
    return s_screen;
}

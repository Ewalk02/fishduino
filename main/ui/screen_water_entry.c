#include "screen_water_entry.h"

#include "screen_water_tests.h"

#include <stdio.h>
#include <string.h>

#include "water/water_metrics.h"

static lv_obj_t *s_screen;
static lv_obj_t *s_label_status;
static lv_obj_t *s_ta_notes;
static lv_obj_t *s_keyboard;

static uint16_t s_ph_tenths = 78;
static uint16_t s_nh3_tenths = 0;
static uint16_t s_no2_tenths = 0;
static uint16_t s_no3_tenths = 0;

static bool s_ph_valid = true;
static bool s_nh3_valid = true;
static bool s_no2_valid = true;
static bool s_no3_valid = true;

static void hide_self(void)
{
    if (s_screen) {
        lv_obj_add_flag(s_screen, LV_OBJ_FLAG_HIDDEN);
    }
    if (s_keyboard) {
        lv_keyboard_set_textarea(s_keyboard, NULL);
        lv_obj_add_flag(s_keyboard, LV_OBJ_FLAG_HIDDEN);
    }
}

static void refresh_status(void)
{
    if (s_label_status == NULL) {
        return;
    }
    char buf[128];
    snprintf(buf, sizeof(buf), "pH %s%.1f  NH3 %s%.1f  NO2 %s%.1f  NO3 %s%.0f",
             s_ph_valid ? "" : "-", s_ph_valid ? (double)s_ph_tenths / 10.0 : 0.0, s_nh3_valid ? "" : "-",
             s_nh3_valid ? (double)s_nh3_tenths / 10.0 : 0.0, s_no2_valid ? "" : "-",
             s_no2_valid ? (double)s_no2_tenths / 10.0 : 0.0, s_no3_valid ? "" : "-",
             s_no3_valid ? (double)s_no3_tenths / 10.0 : 0.0);
    lv_label_set_text(s_label_status, buf);
}

typedef struct {
    uint16_t *val;
    uint16_t min;
    uint16_t max;
    bool *valid;
} stepper_ctx_t;

static void stepper_inc_cb(lv_event_t *e)
{
    stepper_ctx_t *ctx = lv_event_get_user_data(e);
    if (ctx == NULL || ctx->val == NULL) {
        return;
    }
    if (ctx->valid) {
        *ctx->valid = true;
    }
    if (*ctx->val + 1 <= ctx->max) {
        (*ctx->val)++;
    }
    refresh_status();
}

static void stepper_dec_cb(lv_event_t *e)
{
    stepper_ctx_t *ctx = lv_event_get_user_data(e);
    if (ctx == NULL || ctx->val == NULL) {
        return;
    }
    if (ctx->valid) {
        *ctx->valid = true;
    }
    if (*ctx->val > ctx->min) {
        (*ctx->val)--;
    }
    refresh_status();
}

static void skip_cb(lv_event_t *e)
{
    bool *valid = lv_event_get_user_data(e);
    if (valid) {
        *valid = false;
        refresh_status();
    }
}

static void ta_focus_cb(lv_event_t *e)
{
    lv_obj_t *ta = lv_event_get_target(e);
    if (s_keyboard) {
        lv_keyboard_set_textarea(s_keyboard, ta);
        lv_obj_clear_flag(s_keyboard, LV_OBJ_FLAG_HIDDEN);
    }
}

static void ta_defocus_cb(lv_event_t *e)
{
    (void)e;
    if (s_keyboard) {
        lv_keyboard_set_textarea(s_keyboard, NULL);
        lv_obj_add_flag(s_keyboard, LV_OBJ_FLAG_HIDDEN);
    }
}

static void make_row(lv_obj_t *parent, int y, const char *name, uint16_t *val, uint16_t min, uint16_t max,
                     bool *valid, stepper_ctx_t *ctx_store)
{
    lv_obj_t *lbl = lv_label_create(parent);
    lv_label_set_text(lbl, name);
    lv_obj_align(lbl, LV_ALIGN_TOP_LEFT, 8, y);

    ctx_store->val = val;
    ctx_store->min = min;
    ctx_store->max = max;
    ctx_store->valid = valid;

    lv_obj_t *bd = lv_btn_create(parent);
    lv_obj_set_size(bd, 36, 28);
    lv_obj_align(bd, LV_ALIGN_TOP_LEFT, 100, y - 4);
    lv_label_set_text(lv_label_create(bd), "-");
    lv_obj_add_event_cb(bd, stepper_dec_cb, LV_EVENT_CLICKED, ctx_store);

    lv_obj_t *bi = lv_btn_create(parent);
    lv_obj_set_size(bi, 36, 28);
    lv_obj_align(bi, LV_ALIGN_TOP_LEFT, 140, y - 4);
    lv_label_set_text(lv_label_create(bi), "+");
    lv_obj_add_event_cb(bi, stepper_inc_cb, LV_EVENT_CLICKED, ctx_store);

    lv_obj_t *bs = lv_btn_create(parent);
    lv_obj_set_size(bs, 48, 28);
    lv_obj_align(bs, LV_ALIGN_TOP_LEFT, 184, y - 4);
    lv_label_set_text(lv_label_create(bs), "Skip");
    lv_obj_add_event_cb(bs, skip_cb, LV_EVENT_CLICKED, valid);
}

static void btn_back_cb(lv_event_t *e)
{
    (void)e;
    hide_self();
}

static void btn_save_cb(lv_event_t *e)
{
    (void)e;

    water_test_entry_t entry = {0};
    if (s_ph_valid) {
        entry.ph = (float)s_ph_tenths / 10.0f;
        entry.valid_flags |= WATER_VALID_PH;
    }
    if (s_nh3_valid) {
        entry.ammonia_ppm = (float)s_nh3_tenths / 10.0f;
        entry.valid_flags |= WATER_VALID_AMMONIA;
    }
    if (s_no2_valid) {
        entry.nitrite_ppm = (float)s_no2_tenths / 10.0f;
        entry.valid_flags |= WATER_VALID_NITRITE;
    }
    if (s_no3_valid) {
        entry.nitrate_ppm = (float)s_no3_tenths / 10.0f;
        entry.valid_flags |= WATER_VALID_NITRATE;
    }

    if (s_ta_notes) {
        const char *note = lv_textarea_get_text(s_ta_notes);
        if (note != NULL && note[0] != '\0') {
            strncpy(entry.notes, note, sizeof(entry.notes) - 1);
            entry.valid_flags |= WATER_VALID_NOTES;
        }
    }

    if (water_metrics_add_entry(&entry) == ESP_OK) {
        fishduino_screen_water_tests_refresh();
        hide_self();
    } else if (s_label_status) {
        lv_label_set_text(s_label_status, "Save failed");
    }
}

void fishduino_screen_water_entry_build(lv_obj_t *parent)
{
    s_screen = lv_obj_create(parent);
    lv_obj_set_size(s_screen, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(s_screen, lv_color_black(), 0);
    lv_obj_add_flag(s_screen, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t *title = lv_label_create(s_screen);
    lv_label_set_text(title, "Add Water Test");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 8);

    s_label_status = lv_label_create(s_screen);
    lv_obj_set_width(s_label_status, 440);
    lv_obj_align(s_label_status, LV_ALIGN_TOP_LEFT, 8, 32);

    static stepper_ctx_t ctx_ph, ctx_nh3, ctx_no2, ctx_no3;
    make_row(s_screen, 60, "pH", &s_ph_tenths, 50, 95, &s_ph_valid, &ctx_ph);
    make_row(s_screen, 96, "Ammonia", &s_nh3_tenths, 0, 999, &s_nh3_valid, &ctx_nh3);
    make_row(s_screen, 132, "Nitrite", &s_no2_tenths, 0, 999, &s_no2_valid, &ctx_no2);
    make_row(s_screen, 168, "Nitrate", &s_no3_tenths, 0, 999, &s_no3_valid, &ctx_no3);

    lv_obj_t *nl = lv_label_create(s_screen);
    lv_label_set_text(nl, "Notes:");
    lv_obj_align(nl, LV_ALIGN_TOP_LEFT, 8, 204);

    s_ta_notes = lv_textarea_create(s_screen);
    lv_obj_set_size(s_ta_notes, 300, 32);
    lv_obj_align(s_ta_notes, LV_ALIGN_TOP_LEFT, 70, 200);
    lv_textarea_set_one_line(s_ta_notes, true);
    lv_textarea_set_max_length(s_ta_notes, WATER_TEST_NOTES_LEN - 1);
    lv_obj_add_event_cb(s_ta_notes, ta_focus_cb, LV_EVENT_FOCUSED, NULL);
    lv_obj_add_event_cb(s_ta_notes, ta_defocus_cb, LV_EVENT_DEFOCUSED, NULL);

    s_keyboard = lv_keyboard_create(s_screen);
    lv_obj_set_size(s_keyboard, LV_PCT(100), LV_PCT(40));
    lv_obj_align(s_keyboard, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_add_flag(s_keyboard, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t *btn_save = lv_btn_create(s_screen);
    lv_obj_set_size(btn_save, 100, 36);
    lv_obj_align(btn_save, LV_ALIGN_TOP_LEFT, 8, 250);
    lv_label_set_text(lv_label_create(btn_save), "SAVE");
    lv_obj_add_event_cb(btn_save, btn_save_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *btn_cancel = lv_btn_create(s_screen);
    lv_obj_set_size(btn_cancel, 100, 36);
    lv_obj_align(btn_cancel, LV_ALIGN_TOP_LEFT, 120, 250);
    lv_label_set_text(lv_label_create(btn_cancel), "CANCEL");
    lv_obj_add_event_cb(btn_cancel, btn_back_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *btn_back = lv_btn_create(s_screen);
    lv_obj_set_size(btn_back, 100, 36);
    lv_obj_align(btn_back, LV_ALIGN_BOTTOM_MID, 0, -8);
    lv_label_set_text(lv_label_create(btn_back), "BACK");
    lv_obj_add_event_cb(btn_back, btn_back_cb, LV_EVENT_CLICKED, NULL);

    refresh_status();
}

void fishduino_screen_water_entry_show(void)
{
    if (s_screen == NULL) {
        return;
    }
    lv_obj_clear_flag(s_screen, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_screen);
}

void fishduino_screen_water_entry_hide(void)
{
    hide_self();
}

lv_obj_t *fishduino_screen_water_entry_root(void)
{
    return s_screen;
}

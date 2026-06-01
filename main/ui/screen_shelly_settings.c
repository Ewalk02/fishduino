#include "screen_shelly_settings.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "net/shelly_address.h"
#include "storage/settings_nvs.h"
#include "storage/settings_runtime.h"

static lv_obj_t *s_screen;
static lv_obj_t *s_ta_co2_ip;
static lv_obj_t *s_ta_filter_ip;
static lv_obj_t *s_ta_heater_ip;
static lv_obj_t *s_sw_co2_en;
static lv_obj_t *s_sw_filter_en;
static lv_obj_t *s_sw_heater_en;
static lv_obj_t *s_lbl_co2_sw;
static lv_obj_t *s_lbl_filter_sw;
static lv_obj_t *s_label_status;
static lv_obj_t *s_keyboard;

static int8_t s_co2_switch_id;
static int8_t s_filter_switch_id;
static int8_t s_heater_switch_id;

typedef struct {
    fishduino_shelly_plug_settings_t co2;
    fishduino_shelly_plug_settings_t filter;
    fishduino_shelly_plug_settings_t heater;
} shelly_save_ctx_t;

static bool mutator_shelly_save(fishduino_settings_t *st, void *ctx)
{
    shelly_save_ctx_t *s = (shelly_save_ctx_t *)ctx;
    st->shelly_co2 = s->co2;
    st->shelly_filter = s->filter;
    st->shelly_heater = s->heater;
    return true;
}

static void switch_id_label(lv_obj_t *lbl, int8_t id)
{
    char buf[16];
    snprintf(buf, sizeof(buf), "Switch id: %d", (int)id);
    lv_label_set_text(lbl, buf);
}

static void btn_co2_sw_dec(lv_event_t *e)
{
    (void)e;
    if (s_co2_switch_id > 0) {
        s_co2_switch_id--;
    }
    switch_id_label(s_lbl_co2_sw, s_co2_switch_id);
}

static void btn_co2_sw_inc(lv_event_t *e)
{
    (void)e;
    if (s_co2_switch_id < 3) {
        s_co2_switch_id++;
    }
    switch_id_label(s_lbl_co2_sw, s_co2_switch_id);
}

static void btn_filter_sw_dec(lv_event_t *e)
{
    (void)e;
    if (s_filter_switch_id > 0) {
        s_filter_switch_id--;
    }
    switch_id_label(s_lbl_filter_sw, s_filter_switch_id);
}

static void btn_filter_sw_inc(lv_event_t *e)
{
    (void)e;
    if (s_filter_switch_id < 3) {
        s_filter_switch_id++;
    }
    switch_id_label(s_lbl_filter_sw, s_filter_switch_id);
}

static lv_obj_t *s_lbl_heater_sw;

static void btn_heater_sw_dec(lv_event_t *e)
{
    (void)e;
    if (s_heater_switch_id > 0) {
        s_heater_switch_id--;
    }
    switch_id_label(s_lbl_heater_sw, s_heater_switch_id);
}

static void btn_heater_sw_inc(lv_event_t *e)
{
    (void)e;
    if (s_heater_switch_id < 3) {
        s_heater_switch_id++;
    }
    switch_id_label(s_lbl_heater_sw, s_heater_switch_id);
}

static void ta_focus_cb(lv_event_t *e)
{
    lv_obj_t *ta = lv_event_get_target(e);
    if (s_keyboard != NULL) {
        lv_keyboard_set_textarea(s_keyboard, ta);
        lv_obj_clear_flag(s_keyboard, LV_OBJ_FLAG_HIDDEN);
    }
}

static void ta_defocus_cb(lv_event_t *e)
{
    (void)e;
    if (s_keyboard != NULL) {
        lv_keyboard_set_textarea(s_keyboard, NULL);
        lv_obj_add_flag(s_keyboard, LV_OBJ_FLAG_HIDDEN);
    }
}

static void btn_back_cb(lv_event_t *e)
{
    (void)e;
    fishduino_screen_shelly_hide();
}

static void btn_save_cb(lv_event_t *e)
{
    (void)e;

    const char *co2_ip = lv_textarea_get_text(s_ta_co2_ip);
    const char *filter_ip = lv_textarea_get_text(s_ta_filter_ip);
    const char *heater_ip = lv_textarea_get_text(s_ta_heater_ip);
    bool co2_en = lv_obj_has_state(s_sw_co2_en, LV_STATE_CHECKED);
    bool filter_en = lv_obj_has_state(s_sw_filter_en, LV_STATE_CHECKED);
    bool heater_en = lv_obj_has_state(s_sw_heater_en, LV_STATE_CHECKED);

    char err[64];
    if (fishduino_shelly_address_error(co2_ip, !co2_en, err, sizeof(err))) {
        char msg[80];
        snprintf(msg, sizeof(msg), "CO2: %s", err);
        lv_label_set_text(s_label_status, msg);
        return;
    }
    if (fishduino_shelly_address_error(filter_ip, !filter_en, err, sizeof(err))) {
        char msg[80];
        snprintf(msg, sizeof(msg), "Filter: %s", err);
        lv_label_set_text(s_label_status, msg);
        return;
    }
    if (fishduino_shelly_address_error(heater_ip, !heater_en, err, sizeof(err))) {
        char msg[80];
        snprintf(msg, sizeof(msg), "Heater: %s", err);
        lv_label_set_text(s_label_status, msg);
        return;
    }

    fishduino_settings_t preview = {0};
    if (fishduino_settings_get_snapshot(&preview)) {
        strncpy(preview.shelly_co2.ip, co2_ip != NULL ? co2_ip : "", sizeof(preview.shelly_co2.ip) - 1);
        strncpy(preview.shelly_filter.ip, filter_ip != NULL ? filter_ip : "", sizeof(preview.shelly_filter.ip) - 1);
        strncpy(preview.shelly_heater.ip, heater_ip != NULL ? heater_ip : "", sizeof(preview.shelly_heater.ip) - 1);
        preview.shelly_co2.enabled = co2_en;
        preview.shelly_filter.enabled = filter_en;
        preview.shelly_heater.enabled = heater_en;
        preview.shelly_co2.switch_id = s_co2_switch_id;
        preview.shelly_filter.switch_id = s_filter_switch_id;
        preview.shelly_heater.switch_id = s_heater_switch_id;
        if (fishduino_shelly_plugs_config_error(&preview, err, sizeof(err))) {
            lv_label_set_text(s_label_status, err);
            return;
        }
    }

    shelly_save_ctx_t save = {0};
    fishduino_settings_t cur;
    if (!fishduino_settings_get_snapshot(&cur)) {
        lv_label_set_text(s_label_status, "Settings unavailable");
        return;
    }
    save.co2 = cur.shelly_co2;
    save.filter = cur.shelly_filter;
    save.heater = cur.shelly_heater;
    strncpy(save.co2.ip, co2_ip != NULL ? co2_ip : "", sizeof(save.co2.ip) - 1);
    strncpy(save.filter.ip, filter_ip != NULL ? filter_ip : "", sizeof(save.filter.ip) - 1);
    strncpy(save.heater.ip, heater_ip != NULL ? heater_ip : "", sizeof(save.heater.ip) - 1);
    save.co2.enabled = co2_en;
    save.filter.enabled = filter_en;
    save.heater.enabled = heater_en;
    save.co2.switch_id = s_co2_switch_id;
    save.filter.switch_id = s_filter_switch_id;
    save.heater.switch_id = s_heater_switch_id;

    if (fishduino_settings_update(mutator_shelly_save, &save, true)) {
        lv_label_set_text(s_label_status, "Saved — active now (NVS)");
    } else {
        lv_label_set_text(s_label_status, "Save failed");
    }
}

void fishduino_screen_shelly_build(lv_obj_t *parent)
{
    s_screen = lv_obj_create(parent);
    lv_obj_set_size(s_screen, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(s_screen, lv_color_black(), 0);
    lv_obj_add_flag(s_screen, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t *title = lv_label_create(s_screen);
    lv_label_set_text(title, "Shelly Settings");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 4);

    s_label_status = lv_label_create(s_screen);
    lv_label_set_text(s_label_status, "");
    lv_obj_set_width(s_label_status, 440);
    lv_obj_align(s_label_status, LV_ALIGN_TOP_MID, 0, 24);

    lv_obj_t *lc = lv_label_create(s_screen);
    lv_label_set_text(lc, "CO2 IP:");
    lv_obj_align(lc, LV_ALIGN_TOP_LEFT, 8, 48);

    s_ta_co2_ip = lv_textarea_create(s_screen);
    lv_obj_set_size(s_ta_co2_ip, 200, 32);
    lv_obj_align(s_ta_co2_ip, LV_ALIGN_TOP_LEFT, 70, 44);
    lv_textarea_set_one_line(s_ta_co2_ip, true);
    lv_textarea_set_max_length(s_ta_co2_ip, FISHDUINO_IP_LEN - 1);
    lv_obj_add_event_cb(s_ta_co2_ip, ta_focus_cb, LV_EVENT_FOCUSED, NULL);
    lv_obj_add_event_cb(s_ta_co2_ip, ta_defocus_cb, LV_EVENT_DEFOCUSED, NULL);

    s_sw_co2_en = lv_switch_create(s_screen);
    lv_obj_align(s_sw_co2_en, LV_ALIGN_TOP_LEFT, 280, 48);

    lv_obj_t *lf = lv_label_create(s_screen);
    lv_label_set_text(lf, "Filter IP:");
    lv_obj_align(lf, LV_ALIGN_TOP_LEFT, 8, 88);

    s_ta_filter_ip = lv_textarea_create(s_screen);
    lv_obj_set_size(s_ta_filter_ip, 200, 32);
    lv_obj_align(s_ta_filter_ip, LV_ALIGN_TOP_LEFT, 70, 84);
    lv_textarea_set_one_line(s_ta_filter_ip, true);
    lv_textarea_set_max_length(s_ta_filter_ip, FISHDUINO_IP_LEN - 1);
    lv_obj_add_event_cb(s_ta_filter_ip, ta_focus_cb, LV_EVENT_FOCUSED, NULL);
    lv_obj_add_event_cb(s_ta_filter_ip, ta_defocus_cb, LV_EVENT_DEFOCUSED, NULL);

    s_sw_filter_en = lv_switch_create(s_screen);
    lv_obj_align(s_sw_filter_en, LV_ALIGN_TOP_LEFT, 280, 88);

    lv_obj_t *lh = lv_label_create(s_screen);
    lv_label_set_text(lh, "Heater IP:");
    lv_obj_align(lh, LV_ALIGN_TOP_LEFT, 8, 128);

    s_ta_heater_ip = lv_textarea_create(s_screen);
    lv_obj_set_size(s_ta_heater_ip, 200, 32);
    lv_obj_align(s_ta_heater_ip, LV_ALIGN_TOP_LEFT, 70, 124);
    lv_textarea_set_one_line(s_ta_heater_ip, true);
    lv_textarea_set_max_length(s_ta_heater_ip, FISHDUINO_IP_LEN - 1);
    lv_obj_add_event_cb(s_ta_heater_ip, ta_focus_cb, LV_EVENT_FOCUSED, NULL);
    lv_obj_add_event_cb(s_ta_heater_ip, ta_defocus_cb, LV_EVENT_DEFOCUSED, NULL);

    s_sw_heater_en = lv_switch_create(s_screen);
    lv_obj_align(s_sw_heater_en, LV_ALIGN_TOP_LEFT, 280, 128);

    s_lbl_co2_sw = lv_label_create(s_screen);
    lv_obj_align(s_lbl_co2_sw, LV_ALIGN_TOP_LEFT, 8, 168);
    switch_id_label(s_lbl_co2_sw, 0);

    lv_obj_t *b1 = lv_btn_create(s_screen);
    lv_obj_set_size(b1, 36, 28);
    lv_obj_align(b1, LV_ALIGN_TOP_LEFT, 120, 164);
    lv_label_set_text(lv_label_create(b1), "-");
    lv_obj_add_event_cb(b1, btn_co2_sw_dec, LV_EVENT_CLICKED, NULL);

    lv_obj_t *b2 = lv_btn_create(s_screen);
    lv_obj_set_size(b2, 36, 28);
    lv_obj_align(b2, LV_ALIGN_TOP_LEFT, 160, 164);
    lv_label_set_text(lv_label_create(b2), "+");
    lv_obj_add_event_cb(b2, btn_co2_sw_inc, LV_EVENT_CLICKED, NULL);

    s_lbl_filter_sw = lv_label_create(s_screen);
    lv_obj_align(s_lbl_filter_sw, LV_ALIGN_TOP_LEFT, 8, 208);
    switch_id_label(s_lbl_filter_sw, 0);

    lv_obj_t *b3 = lv_btn_create(s_screen);
    lv_obj_set_size(b3, 36, 28);
    lv_obj_align(b3, LV_ALIGN_TOP_LEFT, 120, 204);
    lv_label_set_text(lv_label_create(b3), "-");
    lv_obj_add_event_cb(b3, btn_filter_sw_dec, LV_EVENT_CLICKED, NULL);

    lv_obj_t *b4 = lv_btn_create(s_screen);
    lv_obj_set_size(b4, 36, 28);
    lv_obj_align(b4, LV_ALIGN_TOP_LEFT, 160, 204);
    lv_label_set_text(lv_label_create(b4), "+");
    lv_obj_add_event_cb(b4, btn_filter_sw_inc, LV_EVENT_CLICKED, NULL);

    s_lbl_heater_sw = lv_label_create(s_screen);
    lv_obj_align(s_lbl_heater_sw, LV_ALIGN_TOP_LEFT, 8, 248);
    switch_id_label(s_lbl_heater_sw, 0);

    lv_obj_t *b5 = lv_btn_create(s_screen);
    lv_obj_set_size(b5, 36, 28);
    lv_obj_align(b5, LV_ALIGN_TOP_LEFT, 120, 244);
    lv_label_set_text(lv_label_create(b5), "-");
    lv_obj_add_event_cb(b5, btn_heater_sw_dec, LV_EVENT_CLICKED, NULL);

    lv_obj_t *b6 = lv_btn_create(s_screen);
    lv_obj_set_size(b6, 36, 28);
    lv_obj_align(b6, LV_ALIGN_TOP_LEFT, 160, 244);
    lv_label_set_text(lv_label_create(b6), "+");
    lv_obj_add_event_cb(b6, btn_heater_sw_inc, LV_EVENT_CLICKED, NULL);

    lv_obj_t *btn_save = lv_btn_create(s_screen);
    lv_obj_set_size(btn_save, 160, 40);
    lv_obj_align(btn_save, LV_ALIGN_TOP_MID, 0, 290);
    lv_label_set_text(lv_label_create(btn_save), "SAVE");
    lv_obj_add_event_cb(btn_save, btn_save_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *btn_back = lv_btn_create(s_screen);
    lv_obj_set_size(btn_back, 100, 36);
    lv_obj_align(btn_back, LV_ALIGN_BOTTOM_MID, 0, -4);
    lv_label_set_text(lv_label_create(btn_back), "BACK");
    lv_obj_add_event_cb(btn_back, btn_back_cb, LV_EVENT_CLICKED, NULL);

    s_keyboard = lv_keyboard_create(s_screen);
    lv_obj_set_size(s_keyboard, LV_PCT(100), 140);
    lv_obj_align(s_keyboard, LV_ALIGN_BOTTOM_MID, 0, -44);
    lv_obj_add_flag(s_keyboard, LV_OBJ_FLAG_HIDDEN);
}

void fishduino_screen_shelly_show(void)
{
    if (s_screen == NULL) {
        return;
    }

    fishduino_settings_t st;
    if (fishduino_settings_get_snapshot(&st)) {
        lv_textarea_set_text(s_ta_co2_ip, st.shelly_co2.ip);
        lv_textarea_set_text(s_ta_filter_ip, st.shelly_filter.ip);
        lv_textarea_set_text(s_ta_heater_ip, st.shelly_heater.ip);
        if (st.shelly_co2.enabled) {
            lv_obj_add_state(s_sw_co2_en, LV_STATE_CHECKED);
        } else {
            lv_obj_clear_state(s_sw_co2_en, LV_STATE_CHECKED);
        }
        if (st.shelly_filter.enabled) {
            lv_obj_add_state(s_sw_filter_en, LV_STATE_CHECKED);
        } else {
            lv_obj_clear_state(s_sw_filter_en, LV_STATE_CHECKED);
        }
        if (st.shelly_heater.enabled) {
            lv_obj_add_state(s_sw_heater_en, LV_STATE_CHECKED);
        } else {
            lv_obj_clear_state(s_sw_heater_en, LV_STATE_CHECKED);
        }
        s_co2_switch_id = st.shelly_co2.switch_id;
        s_filter_switch_id = st.shelly_filter.switch_id;
        s_heater_switch_id = st.shelly_heater.switch_id;
        switch_id_label(s_lbl_co2_sw, s_co2_switch_id);
        switch_id_label(s_lbl_filter_sw, s_filter_switch_id);
        switch_id_label(s_lbl_heater_sw, s_heater_switch_id);
    }

    lv_label_set_text(s_label_status, "CO2/filter/heater plug config (NVS)");
    lv_obj_clear_flag(s_screen, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_screen);
}

void fishduino_screen_shelly_hide(void)
{
    if (s_screen != NULL) {
        lv_obj_add_flag(s_screen, LV_OBJ_FLAG_HIDDEN);
    }
    if (s_keyboard != NULL) {
        lv_keyboard_set_textarea(s_keyboard, NULL);
        lv_obj_add_flag(s_keyboard, LV_OBJ_FLAG_HIDDEN);
    }
}

lv_obj_t *fishduino_screen_shelly_root(void)
{
    return s_screen;
}

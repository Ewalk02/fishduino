#include "screen_wifi_settings.h"

#include <stdio.h>
#include <string.h>

#include "net/wifi_manager.h"
#include "storage/wifi_creds_nvs.h"

static lv_obj_t *s_wifi_screen;
static lv_obj_t *s_ta_ssid;
static lv_obj_t *s_ta_password;
static lv_obj_t *s_label_status;
static lv_obj_t *s_keyboard;

static void hide_wifi_screen(void)
{
    if (s_wifi_screen != NULL) {
        lv_obj_add_flag(s_wifi_screen, LV_OBJ_FLAG_HIDDEN);
    }
    if (s_keyboard != NULL) {
        lv_keyboard_set_textarea(s_keyboard, NULL);
        lv_obj_add_flag(s_keyboard, LV_OBJ_FLAG_HIDDEN);
    }
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
    fishduino_screen_wifi_hide();
}

static void btn_save_cb(lv_event_t *e)
{
    (void)e;

    const char *ssid = lv_textarea_get_text(s_ta_ssid);
    const char *pass = lv_textarea_get_text(s_ta_password);

    if (ssid == NULL || ssid[0] == '\0') {
        if (s_label_status != NULL) {
            lv_label_set_text(s_label_status, "SSID required");
        }
        return;
    }

    fishduino_wifi_apply_credentials_async(ssid, pass != NULL ? pass : "");

    if (s_label_status != NULL) {
        lv_label_set_text(s_label_status, "Saving, reconnecting...");
    }
}

void fishduino_screen_wifi_build(lv_obj_t *parent)
{
    s_wifi_screen = lv_obj_create(parent);
    lv_obj_set_size(s_wifi_screen, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(s_wifi_screen, lv_color_black(), 0);
    lv_obj_add_flag(s_wifi_screen, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t *title = lv_label_create(s_wifi_screen);
    lv_label_set_text(title, "Wi-Fi Settings");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 8);

    s_label_status = lv_label_create(s_wifi_screen);
    lv_label_set_text(s_label_status, fishduino_wifi_status_text());
    lv_obj_set_width(s_label_status, 440);
    lv_obj_set_style_text_align(s_label_status, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(s_label_status, LV_ALIGN_TOP_MID, 0, 32);

    lv_obj_t *lbl_ssid = lv_label_create(s_wifi_screen);
    lv_label_set_text(lbl_ssid, "Network name (SSID):");
    lv_obj_align(lbl_ssid, LV_ALIGN_TOP_LEFT, 12, 58);

    s_ta_ssid = lv_textarea_create(s_wifi_screen);
    lv_obj_set_size(s_ta_ssid, 440, 40);
    lv_obj_align(s_ta_ssid, LV_ALIGN_TOP_LEFT, 12, 78);
    lv_textarea_set_one_line(s_ta_ssid, true);
    lv_textarea_set_max_length(s_ta_ssid, FISHDUINO_WIFI_SSID_MAX);
    lv_textarea_set_placeholder_text(s_ta_ssid, "SSID");
    lv_obj_add_event_cb(s_ta_ssid, ta_focus_cb, LV_EVENT_FOCUSED, NULL);
    lv_obj_add_event_cb(s_ta_ssid, ta_defocus_cb, LV_EVENT_DEFOCUSED, NULL);

    lv_obj_t *lbl_pass = lv_label_create(s_wifi_screen);
    lv_label_set_text(lbl_pass, "Password:");
    lv_obj_align(lbl_pass, LV_ALIGN_TOP_LEFT, 12, 128);

    s_ta_password = lv_textarea_create(s_wifi_screen);
    lv_obj_set_size(s_ta_password, 440, 40);
    lv_obj_align(s_ta_password, LV_ALIGN_TOP_LEFT, 12, 148);
    lv_textarea_set_one_line(s_ta_password, true);
    lv_textarea_set_password_mode(s_ta_password, true);
    lv_textarea_set_max_length(s_ta_password, FISHDUINO_WIFI_PASS_MAX);
    lv_textarea_set_placeholder_text(s_ta_password, "Password");
    lv_obj_add_event_cb(s_ta_password, ta_focus_cb, LV_EVENT_FOCUSED, NULL);
    lv_obj_add_event_cb(s_ta_password, ta_defocus_cb, LV_EVENT_DEFOCUSED, NULL);

    lv_obj_t *btn_save = lv_btn_create(s_wifi_screen);
    lv_obj_set_size(btn_save, 200, 44);
    lv_obj_align(btn_save, LV_ALIGN_TOP_MID, 0, 200);
    lv_obj_t *lsave = lv_label_create(btn_save);
    lv_label_set_text(lsave, "SAVE & CONNECT");
    lv_obj_center(lsave);
    lv_obj_add_event_cb(btn_save, btn_save_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *btn_back = lv_btn_create(s_wifi_screen);
    lv_obj_set_size(btn_back, 120, 40);
    lv_obj_align(btn_back, LV_ALIGN_BOTTOM_MID, 0, -8);
    lv_obj_t *lback = lv_label_create(btn_back);
    lv_label_set_text(lback, "BACK");
    lv_obj_center(lback);
    lv_obj_add_event_cb(btn_back, btn_back_cb, LV_EVENT_CLICKED, NULL);

    s_keyboard = lv_keyboard_create(s_wifi_screen);
    lv_obj_set_size(s_keyboard, LV_PCT(100), 160);
    lv_obj_align(s_keyboard, LV_ALIGN_BOTTOM_MID, 0, -52);
    lv_obj_add_flag(s_keyboard, LV_OBJ_FLAG_HIDDEN);
}

void fishduino_screen_wifi_show(void)
{
    if (s_wifi_screen == NULL) {
        return;
    }

    char ssid[FISHDUINO_WIFI_SSID_MAX + 1];
    char password[FISHDUINO_WIFI_PASS_MAX + 1];
    fishduino_wifi_get_credentials(ssid, sizeof(ssid), password, sizeof(password));

    lv_textarea_set_text(s_ta_ssid, ssid);
    lv_textarea_set_text(s_ta_password, password);

    if (s_label_status != NULL) {
        fishduino_wifi_status_t wst;
        fishduino_wifi_get_status(&wst);
        char buf[160];
        snprintf(buf, sizeof(buf), "%s", fishduino_wifi_status_text());
        if (wst.kind == FISHDUINO_WIFI_STATUS_DISCONNECTED && wst.reason_text[0] != '\0') {
            snprintf(buf, sizeof(buf), "%s\nReason: %s", fishduino_wifi_status_text(), wst.reason_text);
        }
        lv_label_set_text(s_label_status, buf);
    }

    lv_obj_clear_flag(s_wifi_screen, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_wifi_screen);
}

void fishduino_screen_wifi_hide(void)
{
    hide_wifi_screen();
}

lv_obj_t *fishduino_screen_wifi_root(void)
{
    return s_wifi_screen;
}

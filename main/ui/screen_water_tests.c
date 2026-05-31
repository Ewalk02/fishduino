#include "screen_water_tests.h"

#include "screen_maint_tracker.h"
#include "screen_water_entry.h"
#include "screen_water_history.h"

#include <stdio.h>
#include <time.h>

#include "maint_tracker/maint_tracker.h"
#include "water/water_alerts.h"
#include "water/water_metrics.h"

static lv_obj_t *s_screen;
static lv_obj_t *s_label_latest;
static lv_obj_t *s_label_alert;
static lv_obj_t *s_label_maint;

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

static void btn_add_cb(lv_event_t *e)
{
    (void)e;
    fishduino_screen_water_entry_show();
}

static void btn_history_cb(lv_event_t *e)
{
    (void)e;
    fishduino_screen_water_history_show();
}

static void btn_reminders_cb(lv_event_t *e)
{
    (void)e;
    fishduino_screen_maint_tracker_show();
}

void fishduino_screen_water_tests_build(lv_obj_t *parent)
{
    s_screen = lv_obj_create(parent);
    lv_obj_set_size(s_screen, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(s_screen, lv_color_black(), 0);
    lv_obj_add_flag(s_screen, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t *title = lv_label_create(s_screen);
    lv_label_set_text(title, "Water Tests");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 8);

    s_label_latest = lv_label_create(s_screen);
    lv_obj_set_width(s_label_latest, 440);
    lv_label_set_long_mode(s_label_latest, LV_LABEL_LONG_WRAP);
    lv_obj_align(s_label_latest, LV_ALIGN_TOP_LEFT, 12, 36);

    s_label_alert = lv_label_create(s_screen);
    lv_obj_set_width(s_label_alert, 440);
    lv_obj_set_style_text_color(s_label_alert, lv_palette_main(LV_PALETTE_ORANGE), 0);
    lv_obj_align(s_label_alert, LV_ALIGN_TOP_LEFT, 12, 120);

    s_label_maint = lv_label_create(s_screen);
    lv_obj_set_width(s_label_maint, 440);
    lv_label_set_long_mode(s_label_maint, LV_LABEL_LONG_WRAP);
    lv_obj_align(s_label_maint, LV_ALIGN_TOP_LEFT, 12, 150);

    lv_obj_t *btn_add = lv_btn_create(s_screen);
    lv_obj_set_size(btn_add, 140, 36);
    lv_obj_align(btn_add, LV_ALIGN_TOP_LEFT, 12, 200);
    lv_label_set_text(lv_label_create(btn_add), "Add Test");
    lv_obj_add_event_cb(btn_add, btn_add_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *btn_hist = lv_btn_create(s_screen);
    lv_obj_set_size(btn_hist, 140, 36);
    lv_obj_align(btn_hist, LV_ALIGN_TOP_LEFT, 160, 200);
    lv_label_set_text(lv_label_create(btn_hist), "History");
    lv_obj_add_event_cb(btn_hist, btn_history_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *btn_rem = lv_btn_create(s_screen);
    lv_obj_set_size(btn_rem, 140, 36);
    lv_obj_align(btn_rem, LV_ALIGN_TOP_LEFT, 12, 246);
    lv_label_set_text(lv_label_create(btn_rem), "Reminders");
    lv_obj_add_event_cb(btn_rem, btn_reminders_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *btn_back = lv_btn_create(s_screen);
    lv_obj_set_size(btn_back, 100, 36);
    lv_obj_align(btn_back, LV_ALIGN_BOTTOM_MID, 0, -8);
    lv_label_set_text(lv_label_create(btn_back), "BACK");
    lv_obj_add_event_cb(btn_back, btn_back_cb, LV_EVENT_CLICKED, NULL);
}

void fishduino_screen_water_tests_refresh(void)
{
    if (s_label_latest == NULL) {
        return;
    }

    water_test_entry_t e;
    char buf[256];
    if (water_metrics_get_latest(&e) != ESP_OK) {
        lv_label_set_text(s_label_latest, "No water tests yet.\nTap Add Test to log pH and nutrients.");
        if (s_label_alert) {
            lv_label_set_text(s_label_alert, "");
        }
    } else {
        char date[24];
        if (e.timestamp_unix <= 0 || (e.valid_flags & WATER_FLAG_TIME_UNKNOWN)) {
            snprintf(date, sizeof(date), "unknown");
        } else {
            time_t t = (time_t)e.timestamp_unix;
            struct tm tm_local;
            if (localtime_r(&t, &tm_local) != NULL) {
                strftime(date, sizeof(date), "%Y-%m-%d", &tm_local);
            } else {
                snprintf(date, sizeof(date), "-");
            }
        }
        snprintf(buf, sizeof(buf),
                 "pH: %.1f\nAmmonia: %.1f ppm\nNitrite: %.1f ppm\nNitrate: %.0f ppm\nLast test: %s",
                 (e.valid_flags & WATER_VALID_PH) ? (double)e.ph : 0.0,
                 (e.valid_flags & WATER_VALID_AMMONIA) ? (double)e.ammonia_ppm : 0.0,
                 (e.valid_flags & WATER_VALID_NITRITE) ? (double)e.nitrite_ppm : 0.0,
                 (e.valid_flags & WATER_VALID_NITRATE) ? (double)e.nitrate_ppm : 0.0, date);
        lv_label_set_text(s_label_latest, buf);

        char alert[96];
        water_alert_level_t lvl = water_alerts_classify(&e, alert, sizeof(alert));
        if (lvl != WATER_ALERT_OK && alert[0] != '\0') {
            snprintf(buf, sizeof(buf), "Alert (%s): %s", water_alerts_level_text(lvl), alert);
            lv_label_set_text(s_label_alert, buf);
            if (lvl == WATER_ALERT_WARNING) {
                lv_obj_set_style_text_color(s_label_alert, lv_palette_main(LV_PALETTE_RED), 0);
            } else {
                lv_obj_set_style_text_color(s_label_alert, lv_palette_main(LV_PALETTE_ORANGE), 0);
            }
        } else {
            lv_label_set_text(s_label_alert, "");
        }
    }

    if (s_label_maint) {
        maintenance_task_t due[MAINT_TASK_COUNT];
        size_t n = 0;
        maintenance_tracker_get_due_tasks(due, MAINT_TASK_COUNT, &n);
        if (n == 0) {
            lv_label_set_text(s_label_maint, "Reminders: all up to date");
        } else {
            snprintf(buf, sizeof(buf), "Reminders: %zu due/overdue", n);
            lv_label_set_text(s_label_maint, buf);
        }
    }
}

void fishduino_screen_water_tests_show(void)
{
    if (s_screen == NULL) {
        return;
    }
    fishduino_screen_water_tests_refresh();
    lv_obj_clear_flag(s_screen, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_screen);
}

void fishduino_screen_water_tests_hide(void)
{
    hide_self();
}

lv_obj_t *fishduino_screen_water_tests_root(void)
{
    return s_screen;
}

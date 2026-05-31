#include "screen_maint_tracker.h"

#include "screen_water_entry.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

#include "maint_tracker/maint_tracker.h"
#include "water/water_metrics.h"

typedef struct {
    maintenance_task_id_t id;
    uint32_t days;
} maint_snooze_ctx_t;

static lv_obj_t *s_screen;
static lv_obj_t *s_label_list;
static lv_obj_t *s_label_prompt;
static maintenance_task_id_t s_pending_water_prompt = MAINT_TASK_COUNT;

static void hide_self(void)
{
    if (s_screen) {
        lv_obj_add_flag(s_screen, LV_OBJ_FLAG_HIDDEN);
    }
}

static void format_date(int64_t ts, char *buf, size_t len)
{
    if (ts <= 0 || ts < 1704067200LL) {
        snprintf(buf, len, "-");
        return;
    }
    time_t t = (time_t)ts;
    struct tm tm_local;
    if (localtime_r(&t, &tm_local) == NULL) {
        snprintf(buf, len, "-");
        return;
    }
    strftime(buf, len, "%Y-%m-%d", &tm_local);
}

static void refresh_list(void)
{
    if (s_label_list == NULL) {
        return;
    }

    maintenance_task_t tasks[MAINT_TASK_COUNT];
    size_t count = 0;
    maintenance_tracker_get_tasks(tasks, MAINT_TASK_COUNT, &count);

    char buf[640] = {0};
    for (size_t i = 0; i < count; i++) {
        const maintenance_task_t *t = &tasks[i];
        if (!t->enabled) {
            continue;
        }
        char last[16];
        char due[16];
        maintenance_tracker_status_t st = maintenance_tracker_task_status(t);
        if (st == MAINT_TRACKER_STATUS_DONE_NO_TIME || st == MAINT_TRACKER_STATUS_SNOOZED_NO_TIME) {
            snprintf(last, sizeof(last), "-");
            snprintf(due, sizeof(due), "-");
        } else {
            format_date(t->last_completed_unix, last, sizeof(last));
            format_date(t->next_due_unix, due, sizeof(due));
        }
        char line[128];
        snprintf(line, sizeof(line), "[%d] %s\n  last=%s due=%s (%s)\n", (int)t->id, t->name, last, due,
                 maintenance_tracker_status_text(st));
        strncat(buf, line, sizeof(buf) - strlen(buf) - 1);
    }

    if (buf[0] == '\0') {
        lv_label_set_text(s_label_list, "No tasks");
    } else {
        lv_label_set_text(s_label_list, buf);
    }
}

static void btn_back_cb(lv_event_t *e)
{
    (void)e;
    s_pending_water_prompt = MAINT_TASK_COUNT;
    if (s_label_prompt) {
        lv_obj_add_flag(s_label_prompt, LV_OBJ_FLAG_HIDDEN);
    }
    hide_self();
}

static void done_cb(lv_event_t *e)
{
    maintenance_task_id_t id = (maintenance_task_id_t)(intptr_t)lv_event_get_user_data(e);
    if (id == MAINT_TASK_WATER_TEST) {
        water_test_entry_t latest;
        bool has_recent = false;
        if (water_metrics_get_latest(&latest) == ESP_OK && latest.timestamp_unix > 0) {
            has_recent = true;
        }
        if (!has_recent && s_label_prompt) {
            s_pending_water_prompt = id;
            lv_label_set_text(s_label_prompt, "Add water test results now?");
            lv_obj_clear_flag(s_label_prompt, LV_OBJ_FLAG_HIDDEN);
            return;
        }
    }
    maintenance_tracker_mark_done(id);
    refresh_list();
}

static void snooze_cb(lv_event_t *e)
{
    maint_snooze_ctx_t *ctx = lv_event_get_user_data(e);
    if (ctx == NULL) {
        return;
    }
    maintenance_tracker_snooze(ctx->id, ctx->days);
    refresh_list();
}

static void prompt_add_cb(lv_event_t *e)
{
    (void)e;
    s_pending_water_prompt = MAINT_TASK_COUNT;
    if (s_label_prompt) {
        lv_obj_add_flag(s_label_prompt, LV_OBJ_FLAG_HIDDEN);
    }
    fishduino_screen_water_entry_show();
}

static void prompt_skip_cb(lv_event_t *e)
{
    (void)e;
    if (s_pending_water_prompt < MAINT_TASK_COUNT) {
        maintenance_tracker_mark_done(s_pending_water_prompt);
        s_pending_water_prompt = MAINT_TASK_COUNT;
    }
    if (s_label_prompt) {
        lv_obj_add_flag(s_label_prompt, LV_OBJ_FLAG_HIDDEN);
    }
    refresh_list();
}

void fishduino_screen_maint_tracker_build(lv_obj_t *parent)
{
    s_screen = lv_obj_create(parent);
    lv_obj_set_size(s_screen, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(s_screen, lv_color_black(), 0);
    lv_obj_add_flag(s_screen, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t *title = lv_label_create(s_screen);
    lv_label_set_text(title, "Reminders");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 4);

    s_label_list = lv_label_create(s_screen);
    lv_obj_set_width(s_label_list, 420);
    lv_label_set_long_mode(s_label_list, LV_LABEL_LONG_WRAP);
    lv_obj_align(s_label_list, LV_ALIGN_TOP_LEFT, 8, 28);

    s_label_prompt = lv_label_create(s_screen);
    lv_obj_set_width(s_label_prompt, 420);
    lv_obj_set_style_text_color(s_label_prompt, lv_palette_main(LV_PALETTE_CYAN), 0);
    lv_obj_align(s_label_prompt, LV_ALIGN_TOP_LEFT, 8, 280);
    lv_obj_add_flag(s_label_prompt, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t *btn_add = lv_btn_create(s_screen);
    lv_obj_set_size(btn_add, 80, 28);
    lv_obj_align(btn_add, LV_ALIGN_TOP_LEFT, 8, 310);
    lv_label_set_text(lv_label_create(btn_add), "Add Test");
    lv_obj_add_event_cb(btn_add, prompt_add_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *btn_skip = lv_btn_create(s_screen);
    lv_obj_set_size(btn_skip, 80, 28);
    lv_obj_align(btn_skip, LV_ALIGN_TOP_LEFT, 96, 310);
    lv_label_set_text(lv_label_create(btn_skip), "Skip");
    lv_obj_add_event_cb(btn_skip, prompt_skip_cb, LV_EVENT_CLICKED, NULL);

    static maint_snooze_ctx_t snooze_ctxs[MAINT_TASK_COUNT * 3];
    size_t ctx_idx = 0;
    int y = 350;
    for (int id = 0; id < MAINT_TASK_COUNT; id++) {
        lv_obj_t *lbl = lv_label_create(s_screen);
        char name[32];
        snprintf(name, sizeof(name), "T%d Done", id);
        lv_label_set_text(lbl, name);
        lv_obj_align(lbl, LV_ALIGN_TOP_LEFT, 8, y);

        lv_obj_t *bd = lv_btn_create(s_screen);
        lv_obj_set_size(bd, 48, 24);
        lv_obj_align(bd, LV_ALIGN_TOP_LEFT, 60, y - 2);
        lv_label_set_text(lv_label_create(bd), "Done");
        lv_obj_add_event_cb(bd, done_cb, LV_EVENT_CLICKED, (void *)(intptr_t)id);

        const uint32_t days[] = {1, 3, 7};
        for (int s = 0; s < 3 && ctx_idx < sizeof(snooze_ctxs) / sizeof(snooze_ctxs[0]); s++) {
            snooze_ctxs[ctx_idx].id = (maintenance_task_id_t)id;
            snooze_ctxs[ctx_idx].days = days[s];
            lv_obj_t *bs = lv_btn_create(s_screen);
            lv_obj_set_size(bs, 36, 24);
            lv_obj_align(bs, LV_ALIGN_TOP_LEFT, 112 + s * 40, y - 2);
            char sb[8];
            snprintf(sb, sizeof(sb), "%ud", (unsigned)days[s]);
            lv_label_set_text(lv_label_create(bs), sb);
            lv_obj_add_event_cb(bs, snooze_cb, LV_EVENT_CLICKED, &snooze_ctxs[ctx_idx]);
            ctx_idx++;
        }
        y += 28;
    }

    lv_obj_t *btn_back = lv_btn_create(s_screen);
    lv_obj_set_size(btn_back, 100, 36);
    lv_obj_align(btn_back, LV_ALIGN_BOTTOM_MID, 0, -8);
    lv_label_set_text(lv_label_create(btn_back), "BACK");
    lv_obj_add_event_cb(btn_back, btn_back_cb, LV_EVENT_CLICKED, NULL);
}

void fishduino_screen_maint_tracker_refresh(void)
{
    if (s_screen != NULL && lv_obj_has_flag(s_screen, LV_OBJ_FLAG_HIDDEN)) {
        return;
    }
    refresh_list();
}

void fishduino_screen_maint_tracker_show(void)
{
    if (s_screen == NULL) {
        return;
    }
    refresh_list();
    lv_obj_clear_flag(s_screen, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_screen);
}

void fishduino_screen_maint_tracker_hide(void)
{
    hide_self();
}

lv_obj_t *fishduino_screen_maint_tracker_root(void)
{
    return s_screen;
}

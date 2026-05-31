#include "maint_tracker.h"

#include <strings.h>
#include <string.h>

#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "scheduler/scheduler.h"

static const char *TAG = "maint_trk";
static const char *NVS_NS = "fishduino";
static const char *NVS_KEY = "maint_tasks_v1";

#define MAINT_BLOB_VERSION 2U

typedef struct {
    maintenance_task_id_t id;
    char name[48];
    uint32_t interval_days;
    int64_t last_completed_unix;
    int64_t next_due_unix;
    bool enabled;
} maintenance_task_v1_t;

typedef struct {
    uint32_t magic;
    uint16_t version;
    uint16_t count;
    maintenance_task_v1_t tasks[MAINT_TASK_COUNT];
} maint_tasks_blob_v1_t;

typedef struct {
    uint32_t magic;
    uint16_t version;
    uint16_t count;
    maintenance_task_t tasks[MAINT_TASK_COUNT];
} maint_tasks_blob_t;

static maint_tasks_blob_t s_blob;
static bool s_inited;
static bool s_time_recalc_done;

#define MAINT_TIME_VALID_MIN_EPOCH 1704067200LL /* 2024-01-01 00:00:00 UTC */

static bool maint_time_is_valid(int64_t ts)
{
    return ts >= MAINT_TIME_VALID_MIN_EPOCH;
}

static int64_t now_epoch(void)
{
    fishduino_time_snapshot_t snap;
    fishduino_time_snapshot_now(&snap);
    if (!snap.valid_time) {
        return 0;
    }
    int64_t now = (int64_t)snap.epoch_seconds;
    return maint_time_is_valid(now) ? now : 0;
}

static void clear_time_pending(maintenance_task_t *t)
{
    t->completed_without_time = false;
    t->snoozed_without_time = false;
    t->pending_snooze_days = 0;
}

static void default_tasks(maint_tasks_blob_t *blob)
{
    memset(blob, 0, sizeof(*blob));
    blob->magic = 0x4D41494E; /* MAIN */
    blob->version = MAINT_BLOB_VERSION;
    blob->count = MAINT_TASK_COUNT;

    const struct {
        maintenance_task_id_t id;
        const char *name;
        uint32_t days;
    } defs[] = {
        {MAINT_TASK_WATER_TEST, "Check water parameters", 14},
        {MAINT_TASK_FILTER_RINSE, "Rinse filter media", 30},
        {MAINT_TASK_DROP_CHECKER, "Change drop checker fluid", 30},
        {MAINT_TASK_TRIM_PLANTS, "Trim plants", 14},
        {MAINT_TASK_CLEAN_GLASS, "Clean glass", 7},
        {MAINT_TASK_CO2_INSPECTION, "Inspect CO2 tubing/check valve", 30},
        {MAINT_TASK_FILTER_FLOW_CHECK, "Check filter flow", 30},
    };

    for (size_t i = 0; i < sizeof(defs) / sizeof(defs[0]); i++) {
        maintenance_task_t *t = &blob->tasks[defs[i].id];
        t->id = defs[i].id;
        strncpy(t->name, defs[i].name, sizeof(t->name) - 1);
        t->interval_days = defs[i].days;
        t->enabled = true;
        t->last_completed_unix = 0;
        t->next_due_unix = 0;
        clear_time_pending(t);
    }
}

static esp_err_t persist_blob(void)
{
    nvs_handle_t nvh;
    esp_err_t err = nvs_open(NVS_NS, NVS_READWRITE, &nvh);
    if (err != ESP_OK) {
        return err;
    }
    err = nvs_set_blob(nvh, NVS_KEY, &s_blob, sizeof(s_blob));
    if (err == ESP_OK) {
        err = nvs_commit(nvh);
    }
    nvs_close(nvh);
    return err;
}

static void migrate_v1_blob(const maint_tasks_blob_v1_t *old)
{
    memset(&s_blob, 0, sizeof(s_blob));
    s_blob.magic = old->magic;
    s_blob.version = MAINT_BLOB_VERSION;
    s_blob.count = old->count;

    for (size_t i = 0; i < MAINT_TASK_COUNT; i++) {
        const maintenance_task_v1_t *src = &old->tasks[i];
        maintenance_task_t *dst = &s_blob.tasks[i];
        dst->id = src->id;
        memcpy(dst->name, src->name, sizeof(dst->name));
        dst->interval_days = src->interval_days;
        dst->last_completed_unix = src->last_completed_unix;
        dst->next_due_unix = src->next_due_unix;
        dst->enabled = src->enabled;
        clear_time_pending(dst);
    }

    ESP_LOGI(TAG, "Migrated maintenance tasks blob v1 -> v%d", MAINT_BLOB_VERSION);
}

static void repair_task_timestamps(maintenance_task_t *t)
{
    if (t->completed_without_time) {
        t->last_completed_unix = 0;
        t->next_due_unix = 0;
        t->snoozed_without_time = false;
        t->pending_snooze_days = 0;
        return;
    }

    if (t->snoozed_without_time) {
        t->next_due_unix = 0;
        t->completed_without_time = false;
        t->last_completed_unix = 0;
        return;
    }

    if (t->last_completed_unix != 0 && !maint_time_is_valid(t->last_completed_unix)) {
        t->last_completed_unix = 0;
    }

    if (t->next_due_unix == 0 || maint_time_is_valid(t->next_due_unix)) {
        return;
    }

    /* Bogus 1970-era due date: last=0, next=interval*86400 or snooze*86400 from epoch 0. */
    if (t->last_completed_unix == 0) {
        int64_t next = t->next_due_unix;
        if (next == (int64_t)t->interval_days * 86400LL) {
            t->completed_without_time = true;
            t->next_due_unix = 0;
            return;
        }

        uint32_t days = (uint32_t)(next / 86400LL);
        if (days > 0 && next == (int64_t)days * 86400LL) {
            t->snoozed_without_time = true;
            t->pending_snooze_days = days;
            t->next_due_unix = 0;
            return;
        }
    }

    t->next_due_unix = 0;
}

static bool repair_all_tasks(void)
{
    bool changed = false;

    for (size_t i = 0; i < MAINT_TASK_COUNT; i++) {
        maintenance_task_t before = s_blob.tasks[i];
        repair_task_timestamps(&s_blob.tasks[i]);
        if (memcmp(&before, &s_blob.tasks[i], sizeof(before)) != 0) {
            changed = true;
        }
    }

    if (changed) {
        ESP_LOGW(TAG, "Repaired bogus maintenance task timestamps");
    }
    return changed;
}

static void apply_time_sync_to_pending(int64_t now)
{
    if (!maint_time_is_valid(now)) {
        return;
    }

    bool changed = false;

    for (size_t i = 0; i < MAINT_TASK_COUNT; i++) {
        maintenance_task_t *t = &s_blob.tasks[i];
        if (!t->enabled) {
            continue;
        }

        if (t->completed_without_time) {
            t->last_completed_unix = now;
            t->next_due_unix = now + (int64_t)t->interval_days * 86400LL;
            clear_time_pending(t);
            changed = true;
            continue;
        }

        if (t->snoozed_without_time && t->pending_snooze_days > 0) {
            t->next_due_unix = now + (int64_t)t->pending_snooze_days * 86400LL;
            clear_time_pending(t);
            changed = true;
        }
    }

    if (changed) {
        persist_blob();
    }
}

static void recalc_next_due(maintenance_task_t *t, int64_t now)
{
    if (t->completed_without_time || t->snoozed_without_time) {
        return;
    }

    if (maint_time_is_valid(t->last_completed_unix)) {
        t->next_due_unix = t->last_completed_unix + (int64_t)t->interval_days * 86400LL;
    } else if (maint_time_is_valid(now)) {
        t->next_due_unix = now;
    } else {
        t->next_due_unix = 0;
    }
}

static void recalc_all_if_needed(int64_t now)
{
    if (!maint_time_is_valid(now)) {
        return;
    }

    for (size_t i = 0; i < MAINT_TASK_COUNT; i++) {
        maintenance_task_t *t = &s_blob.tasks[i];
        if (!t->enabled || t->completed_without_time || t->snoozed_without_time) {
            continue;
        }
        if (t->next_due_unix == 0) {
            recalc_next_due(t, now);
        }
    }
}

esp_err_t maintenance_tracker_init(void)
{
    if (s_inited) {
        return ESP_OK;
    }

    bool loaded = false;
    nvs_handle_t nvh;
    esp_err_t err = nvs_open(NVS_NS, NVS_READONLY, &nvh);
    if (err == ESP_OK) {
        size_t len = sizeof(s_blob);
        err = nvs_get_blob(nvh, NVS_KEY, &s_blob, &len);
        if (err == ESP_OK && s_blob.magic == 0x4D41494E && s_blob.count == MAINT_TASK_COUNT &&
            s_blob.version == MAINT_BLOB_VERSION && len == sizeof(s_blob)) {
            loaded = true;
        } else {
            maint_tasks_blob_v1_t old = {0};
            len = sizeof(old);
            esp_err_t err_v1 = nvs_get_blob(nvh, NVS_KEY, &old, &len);
            if (err_v1 == ESP_OK && old.magic == 0x4D41494E && old.count == MAINT_TASK_COUNT &&
                len == sizeof(old)) {
                migrate_v1_blob(&old);
                persist_blob();
                loaded = true;
            }
        }
        nvs_close(nvh);
    }

    if (!loaded) {
        ESP_LOGI(TAG, "Creating default maintenance tasks");
        default_tasks(&s_blob);
        persist_blob();
    }

    if (repair_all_tasks()) {
        persist_blob();
    }

    int64_t now = now_epoch();
    if (maint_time_is_valid(now)) {
        apply_time_sync_to_pending(now);
        recalc_all_if_needed(now);
        s_time_recalc_done = true;
    }

    s_inited = true;
    ESP_LOGI(TAG, "Maintenance tracker initialized");
    return ESP_OK;
}

void maintenance_tracker_tick(void)
{
    if (!s_inited || s_time_recalc_done) {
        return;
    }

    int64_t now = now_epoch();
    if (!maint_time_is_valid(now)) {
        return;
    }

    apply_time_sync_to_pending(now);
    recalc_all_if_needed(now);
    persist_blob();
    s_time_recalc_done = true;
    ESP_LOGI(TAG, "Maintenance due dates updated after time sync");
}

maintenance_tracker_status_t maintenance_tracker_task_status(const maintenance_task_t *task)
{
    if (task == NULL || !task->enabled) {
        return MAINT_TRACKER_STATUS_OK;
    }

    if (task->completed_without_time) {
        return MAINT_TRACKER_STATUS_DONE_NO_TIME;
    }
    if (task->snoozed_without_time) {
        return MAINT_TRACKER_STATUS_SNOOZED_NO_TIME;
    }

    int64_t now = now_epoch();
    if (task->next_due_unix <= 0 || !maint_time_is_valid(task->next_due_unix)) {
        return MAINT_TRACKER_STATUS_DUE;
    }
    if (!maint_time_is_valid(now)) {
        return MAINT_TRACKER_STATUS_DUE;
    }

    if (now > task->next_due_unix + 86400LL) {
        return MAINT_TRACKER_STATUS_OVERDUE;
    }
    if (now >= task->next_due_unix) {
        return MAINT_TRACKER_STATUS_DUE;
    }
    return MAINT_TRACKER_STATUS_OK;
}

const char *maintenance_tracker_status_text(maintenance_tracker_status_t st)
{
    switch (st) {
    case MAINT_TRACKER_STATUS_DUE:
        return "due";
    case MAINT_TRACKER_STATUS_OVERDUE:
        return "overdue";
    case MAINT_TRACKER_STATUS_DONE_NO_TIME:
        return "done/time-unknown";
    case MAINT_TRACKER_STATUS_SNOOZED_NO_TIME:
        return "snoozed/time-unknown";
    default:
        return "ok";
    }
}

esp_err_t maintenance_tracker_get_tasks(maintenance_task_t *out, size_t max_tasks, size_t *out_count)
{
    if (!s_inited || out == NULL || out_count == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    size_t n = max_tasks < MAINT_TASK_COUNT ? max_tasks : MAINT_TASK_COUNT;
    memcpy(out, s_blob.tasks, n * sizeof(maintenance_task_t));
    *out_count = n;
    return ESP_OK;
}

esp_err_t maintenance_tracker_mark_done(maintenance_task_id_t id)
{
    if (!s_inited || id >= MAINT_TASK_COUNT) {
        return ESP_ERR_INVALID_ARG;
    }

    int64_t now = now_epoch();
    maintenance_task_t *t = &s_blob.tasks[id];
    clear_time_pending(t);

    if (maint_time_is_valid(now)) {
        t->last_completed_unix = now;
        t->next_due_unix = now + (int64_t)t->interval_days * 86400LL;
    } else {
        t->last_completed_unix = 0;
        t->next_due_unix = 0;
        t->completed_without_time = true;
    }

    ESP_LOGI(TAG, "Task done: %s", t->name);
    return persist_blob();
}

esp_err_t maintenance_tracker_snooze(maintenance_task_id_t id, uint32_t days)
{
    if (!s_inited || id >= MAINT_TASK_COUNT || days == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    int64_t now = now_epoch();
    maintenance_task_t *t = &s_blob.tasks[id];
    clear_time_pending(t);

    if (maint_time_is_valid(now)) {
        t->next_due_unix = now + (int64_t)days * 86400LL;
    } else {
        t->next_due_unix = 0;
        t->snoozed_without_time = true;
        t->pending_snooze_days = days;
    }

    ESP_LOGI(TAG, "Task snoozed %u days: %s", (unsigned)days, t->name);
    return persist_blob();
}

bool maintenance_tracker_any_due(void)
{
    if (!s_inited) {
        return false;
    }

    for (size_t i = 0; i < MAINT_TASK_COUNT; i++) {
        if (!s_blob.tasks[i].enabled) {
            continue;
        }
        maintenance_tracker_status_t st = maintenance_tracker_task_status(&s_blob.tasks[i]);
        if (st == MAINT_TRACKER_STATUS_DUE || st == MAINT_TRACKER_STATUS_OVERDUE) {
            return true;
        }
    }
    return false;
}

esp_err_t maintenance_tracker_get_due_tasks(maintenance_task_t *out, size_t max_tasks, size_t *out_count)
{
    if (!s_inited || out == NULL || out_count == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    size_t written = 0;
    for (size_t i = 0; i < MAINT_TASK_COUNT && written < max_tasks; i++) {
        maintenance_task_t *t = &s_blob.tasks[i];
        if (!t->enabled) {
            continue;
        }
        maintenance_tracker_status_t st = maintenance_tracker_task_status(t);
        if (st == MAINT_TRACKER_STATUS_DUE || st == MAINT_TRACKER_STATUS_OVERDUE) {
            out[written++] = *t;
        }
    }
    *out_count = written;
    return ESP_OK;
}

esp_err_t maintenance_tracker_parse_id(const char *token, maintenance_task_id_t *out)
{
    if (token == NULL || out == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    char *end = NULL;
    long v = strtol(token, &end, 10);
    if (end != token && *end == '\0' && v >= 0 && v < MAINT_TASK_COUNT) {
        *out = (maintenance_task_id_t)v;
        return ESP_OK;
    }

    static const struct {
        const char *alias;
        maintenance_task_id_t id;
    } aliases[] = {
        {"water_test", MAINT_TASK_WATER_TEST},
        {"filter_rinse", MAINT_TASK_FILTER_RINSE},
        {"drop_checker", MAINT_TASK_DROP_CHECKER},
        {"trim_plants", MAINT_TASK_TRIM_PLANTS},
        {"clean_glass", MAINT_TASK_CLEAN_GLASS},
        {"co2_inspection", MAINT_TASK_CO2_INSPECTION},
        {"filter_flow", MAINT_TASK_FILTER_FLOW_CHECK},
    };

    for (size_t i = 0; i < sizeof(aliases) / sizeof(aliases[0]); i++) {
        if (strcasecmp(token, aliases[i].alias) == 0) {
            *out = aliases[i].id;
            return ESP_OK;
        }
    }

    return ESP_ERR_INVALID_ARG;
}

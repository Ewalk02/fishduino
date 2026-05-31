#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    MAINT_TASK_WATER_TEST = 0,
    MAINT_TASK_FILTER_RINSE,
    MAINT_TASK_DROP_CHECKER,
    MAINT_TASK_TRIM_PLANTS,
    MAINT_TASK_CLEAN_GLASS,
    MAINT_TASK_CO2_INSPECTION,
    MAINT_TASK_FILTER_FLOW_CHECK,
    MAINT_TASK_COUNT
} maintenance_task_id_t;

typedef struct {
    maintenance_task_id_t id;
    char name[48];
    uint32_t interval_days;
    int64_t last_completed_unix;
    int64_t next_due_unix;
    bool enabled;
    bool completed_without_time;
    bool snoozed_without_time;
    uint32_t pending_snooze_days;
} maintenance_task_t;

typedef enum {
    MAINT_TRACKER_STATUS_OK = 0,
    MAINT_TRACKER_STATUS_DUE,
    MAINT_TRACKER_STATUS_OVERDUE,
    MAINT_TRACKER_STATUS_DONE_NO_TIME,
    MAINT_TRACKER_STATUS_SNOOZED_NO_TIME,
} maintenance_tracker_status_t;

esp_err_t maintenance_tracker_init(void);

void maintenance_tracker_tick(void);

esp_err_t maintenance_tracker_get_tasks(maintenance_task_t *out, size_t max_tasks, size_t *out_count);

esp_err_t maintenance_tracker_mark_done(maintenance_task_id_t id);

esp_err_t maintenance_tracker_snooze(maintenance_task_id_t id, uint32_t days);

bool maintenance_tracker_any_due(void);

esp_err_t maintenance_tracker_get_due_tasks(maintenance_task_t *out, size_t max_tasks, size_t *out_count);

maintenance_tracker_status_t maintenance_tracker_task_status(const maintenance_task_t *task);

const char *maintenance_tracker_status_text(maintenance_tracker_status_t st);

esp_err_t maintenance_tracker_parse_id(const char *token, maintenance_task_id_t *out);

#ifdef __cplusplus
}
#endif

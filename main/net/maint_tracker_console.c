#include "maint_tracker/maint_tracker.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

#include "esp_console.h"

static void format_ts(int64_t ts, char *buf, size_t len)
{
    if (ts <= 0 || ts < 1704067200LL) {
        snprintf(buf, len, "-");
        return;
    }
    time_t t = (time_t)ts;
    struct tm tm_local;
    if (localtime_r(&t, &tm_local) == NULL) {
        snprintf(buf, len, "%lld", (long long)ts);
        return;
    }
    strftime(buf, len, "%Y-%m-%d", &tm_local);
}

static void print_task(const maintenance_task_t *t)
{
    char last[16];
    char due[16];
    maintenance_tracker_status_t st = maintenance_tracker_task_status(t);

    if (st == MAINT_TRACKER_STATUS_DONE_NO_TIME || st == MAINT_TRACKER_STATUS_SNOOZED_NO_TIME) {
        snprintf(last, sizeof(last), "-");
        snprintf(due, sizeof(due), "-");
    } else {
        format_ts(t->last_completed_unix, last, sizeof(last));
        format_ts(t->next_due_unix, due, sizeof(due));
    }

    printf("  [%d] %s interval=%ud last=%s due=%s status=%s enabled=%s\n", (int)t->id, t->name,
           (unsigned)t->interval_days, last, due, maintenance_tracker_status_text(st),
           t->enabled ? "yes" : "no");
}

static int cmd_maint_list(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    maintenance_task_t tasks[MAINT_TASK_COUNT];
    size_t count = 0;
    maintenance_tracker_get_tasks(tasks, MAINT_TASK_COUNT, &count);
    printf("Maintenance tasks:\n");
    for (size_t i = 0; i < count; i++) {
        print_task(&tasks[i]);
    }
    return 0;
}

static int cmd_maint_due(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    maintenance_task_t tasks[MAINT_TASK_COUNT];
    size_t count = 0;
    maintenance_tracker_get_due_tasks(tasks, MAINT_TASK_COUNT, &count);
    if (count == 0) {
        printf("No due maintenance tasks\n");
        return 0;
    }
    printf("Due/overdue tasks:\n");
    for (size_t i = 0; i < count; i++) {
        print_task(&tasks[i]);
    }
    return 0;
}

static int cmd_maint_done(int argc, char **argv)
{
    if (argc < 2) {
        printf("Usage: maint_done <task_id|alias>\n");
        return 1;
    }

    maintenance_task_id_t id;
    if (maintenance_tracker_parse_id(argv[1], &id) != ESP_OK) {
        printf("Unknown task: %s\n", argv[1]);
        return 1;
    }

    esp_err_t err = maintenance_tracker_mark_done(id);
    printf("maint_done => %s\n", err == ESP_OK ? "ok" : "fail");
    return err == ESP_OK ? 0 : 1;
}

static int cmd_maint_snooze(int argc, char **argv)
{
    if (argc < 3) {
        printf("Usage: maint_snooze <task_id|alias> <days>\n");
        return 1;
    }

    maintenance_task_id_t id;
    if (maintenance_tracker_parse_id(argv[1], &id) != ESP_OK) {
        printf("Unknown task: %s\n", argv[1]);
        return 1;
    }

    unsigned long days = strtoul(argv[2], NULL, 10);
    if (days == 0) {
        printf("Days must be > 0\n");
        return 1;
    }

    esp_err_t err = maintenance_tracker_snooze(id, (uint32_t)days);
    printf("maint_snooze => %s\n", err == ESP_OK ? "ok" : "fail");
    return err == ESP_OK ? 0 : 1;
}

void fishduino_maint_tracker_console_register(void)
{
    const esp_console_cmd_t cmds[] = {
        {.command = "maint_list", .help = "List maintenance tasks", .func = &cmd_maint_list},
        {.command = "maint_due", .help = "List due/overdue tasks", .func = &cmd_maint_due},
        {.command = "maint_done", .help = "Mark task done: maint_done <id|alias>", .func = &cmd_maint_done},
        {.command = "maint_snooze", .help = "Snooze task: maint_snooze <id|alias> <days>", .func = &cmd_maint_snooze},
    };

    for (size_t i = 0; i < sizeof(cmds) / sizeof(cmds[0]); i++) {
        ESP_ERROR_CHECK(esp_console_cmd_register(&cmds[i]));
    }
}

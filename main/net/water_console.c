#include "water/water_metrics.h"
#include "water/water_alerts.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "esp_console.h"

static void format_ts(int64_t ts, char *buf, size_t len)
{
    if (ts <= 0) {
        snprintf(buf, len, "unknown");
        return;
    }
    time_t t = (time_t)ts;
    struct tm tm_local;
    if (localtime_r(&t, &tm_local) == NULL) {
        snprintf(buf, len, "%lld", (long long)ts);
        return;
    }
    strftime(buf, len, "%Y-%m-%d %H:%M", &tm_local);
}

static void print_entry(const water_test_entry_t *e)
{
    char ts[32];
    format_ts(e->timestamp_unix, ts, sizeof(ts));
    printf("time=%s ph=%.1f nh3=%.1f no2=%.1f no3=%.1f flags=0x%02x",
           ts,
           (e->valid_flags & WATER_VALID_PH) ? (double)e->ph : -1.0,
           (e->valid_flags & WATER_VALID_AMMONIA) ? (double)e->ammonia_ppm : -1.0,
           (e->valid_flags & WATER_VALID_NITRITE) ? (double)e->nitrite_ppm : -1.0,
           (e->valid_flags & WATER_VALID_NITRATE) ? (double)e->nitrate_ppm : -1.0,
           (unsigned)e->valid_flags);
    if (e->valid_flags & WATER_VALID_NOTES && e->notes[0] != '\0') {
        printf(" note=\"%s\"", e->notes);
    }
    printf("\n");
}

static int cmd_water_add(int argc, char **argv)
{
    if (argc < 5) {
        printf("Usage: water_add <ph> <ammonia_ppm> <nitrite_ppm> <nitrate_ppm> [note...]\n");
        return 1;
    }

    water_test_entry_t e = {0};
    e.ph = strtof(argv[1], NULL);
    e.ammonia_ppm = strtof(argv[2], NULL);
    e.nitrite_ppm = strtof(argv[3], NULL);
    e.nitrate_ppm = strtof(argv[4], NULL);
    e.valid_flags = WATER_VALID_PH | WATER_VALID_AMMONIA | WATER_VALID_NITRITE | WATER_VALID_NITRATE;

    if (argc >= 6) {
        size_t off = 0;
        bool truncated = false;
        for (int i = 5; i < argc; i++) {
            if (i > 5) {
                if (off >= sizeof(e.notes) - 1) {
                    truncated = true;
                    break;
                }
                e.notes[off++] = ' ';
            }
            size_t room = sizeof(e.notes) - off - 1;
            size_t n = strnlen(argv[i], room);
            if (n < strlen(argv[i])) {
                truncated = true;
            }
            memcpy(e.notes + off, argv[i], n);
            off += n;
            if (off >= sizeof(e.notes) - 1) {
                truncated = true;
                break;
            }
        }
        e.notes[off] = '\0';
        e.valid_flags |= WATER_VALID_NOTES;
        if (truncated) {
            printf("warning: note truncated to %d chars\n", WATER_TEST_NOTES_LEN - 1);
        }
    }

    esp_err_t err = water_metrics_add_entry(&e);
    printf("water_add => %s\n", err == ESP_OK ? "ok" : "fail");
    return err == ESP_OK ? 0 : 1;
}

static int cmd_water_latest(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    water_test_entry_t e;
    if (water_metrics_get_latest(&e) != ESP_OK) {
        printf("No water test entries\n");
        return 0;
    }

    print_entry(&e);
    char alert[96];
    water_alert_level_t lvl = water_alerts_classify(&e, alert, sizeof(alert));
    printf("alert=%s", water_alerts_level_text(lvl));
    if (alert[0] != '\0') {
        printf(" (%s)", alert);
    }
    printf("\n");
    return 0;
}

static int cmd_water_list(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    enum { WATER_LIST_MAX = 20 };

    water_test_entry_t *entries = calloc(WATER_LIST_MAX, sizeof(*entries));
    if (entries == NULL) {
        printf("water_list: out of memory\n");
        return 1;
    }

    size_t count = 0;
    esp_err_t err = water_metrics_get_entries(entries, WATER_LIST_MAX, &count);
    if (err != ESP_OK || count == 0) {
        printf("No entries\n");
        free(entries);
        return 0;
    }

    printf("%zu entries (showing up to %d, newest first):\n", water_metrics_count(), WATER_LIST_MAX);
    for (size_t i = count; i > 0; i--) {
        print_entry(&entries[i - 1]);
    }

    free(entries);
    return 0;
}

static int cmd_water_clear_confirm(int argc, char **argv)
{
    if (argc < 2 || strcmp(argv[1], "confirm") != 0) {
        printf("Usage: water_clear_confirm confirm\n");
        return 1;
    }

    esp_err_t err = water_metrics_clear_all();
    printf("water_clear => %s\n", err == ESP_OK ? "ok" : "fail");
    return err == ESP_OK ? 0 : 1;
}

void fishduino_water_console_register(void)
{
    const esp_console_cmd_t cmds[] = {
        {
            .command = "water_add",
            .help = "Add water test: water_add <ph> <nh3> <no2> <no3> [note...]",
            .func = &cmd_water_add,
        },
        {
            .command = "water_latest",
            .help = "Show latest water test",
            .func = &cmd_water_latest,
        },
        {
            .command = "water_list",
            .help = "List recent water tests",
            .func = &cmd_water_list,
        },
        {
            .command = "water_clear_confirm",
            .help = "Clear all water tests (requires confirm)",
            .func = &cmd_water_clear_confirm,
        },
    };

    for (size_t i = 0; i < sizeof(cmds) / sizeof(cmds[0]); i++) {
        ESP_ERROR_CHECK(esp_console_cmd_register(&cmds[i]));
    }
}

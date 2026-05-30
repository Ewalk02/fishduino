#include "fluval_console.h"

#include <stdio.h>
#include <stdlib.h>

#include "esp_console.h"
#include "fluval/fluval_light.h"
#include "storage/settings_runtime.h"

static int parse_percent_arg(const char *text, uint8_t *out)
{
    if (text == NULL || out == NULL) {
        return 1;
    }
    char *end = NULL;
    long v = strtol(text, &end, 10);
    if (end == text || *end != '\0' || v < 0 || v > 100) {
        return 1;
    }
    *out = (uint8_t)v;
    return 0;
}

static void print_fluval_state(void)
{
    fishduino_fluval_state_t st;
    fishduino_settings_t cfg;

    fishduino_fluval_get_state(&st);
    fishduino_settings_get_snapshot(&cfg);

    printf("Fluval Plant 4.0\n");
    printf("  enabled=%s target=%s mac=%s\n", cfg.fluval.enabled ? "yes" : "no", cfg.fluval.target_name,
           cfg.fluval.target_mac);
    printf("  link=%s connected=%s stale=%s\n", fishduino_fluval_link_text(fishduino_fluval_get_link_status()),
           st.connected ? "yes" : "no", st.stale ? "yes" : "no");
    printf("  mode=%s output=%u%% rssi=%d\n", fishduino_fluval_mode_text(st.mode),
           (unsigned)st.avg_output, st.rssi);
    printf("  channels: P=%u B=%u CW=%u W=%u WW=%u\n", (unsigned)st.pink, (unsigned)st.blue,
           (unsigned)st.cold_white, (unsigned)st.white, (unsigned)st.warm_white);
    printf("  poll=%us stale_timeout=%us\n", (unsigned)cfg.fluval.poll_interval_s,
           (unsigned)cfg.fluval.stale_timeout_s);
    if (st.last_update_ms != 0) {
        printf("  last_update_ms=%lu\n", (unsigned long)st.last_update_ms);
    }
}

static int cmd_fluval_status(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    print_fluval_state();
    return 0;
}

static int cmd_fluval_read(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    esp_err_t err = fishduino_fluval_request_status();
    if (err != ESP_OK) {
        printf("fluval_read failed: %s\n", esp_err_to_name(err));
        return 1;
    }
    printf("Fluval status read queued\n");
    return 0;
}

static int cmd_fluval_manual(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    esp_err_t err = fishduino_fluval_set_mode_manual();
    if (err != ESP_OK) {
        printf("fluval_manual failed: %s\n", esp_err_to_name(err));
        return 1;
    }
    printf("Fluval Manual mode queued\n");
    return 0;
}

static int cmd_fluval_auto(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    esp_err_t err = fishduino_fluval_set_mode_auto();
    if (err != ESP_OK) {
        printf("fluval_auto failed: %s\n", esp_err_to_name(err));
        return 1;
    }
    printf("Fluval Auto mode queued\n");
    return 0;
}

static int cmd_fluval_setall(int argc, char **argv)
{
    if (argc != 2) {
        printf("Usage: fluval_setall <0-100>\n");
        return 1;
    }

    uint8_t percent = 0;
    if (parse_percent_arg(argv[1], &percent) != 0) {
        printf("Invalid percent (0-100)\n");
        return 1;
    }

    esp_err_t err = fishduino_fluval_set_all(percent);
    if (err != ESP_OK) {
        printf("fluval_setall failed: %s\n", esp_err_to_name(err));
        return 1;
    }
    printf("Fluval set-all %u%% queued\n", (unsigned)percent);
    return 0;
}

static int cmd_fluval_set(int argc, char **argv)
{
    if (argc != 6) {
        printf("Usage: fluval_set <pink> <blue> <cold_white> <white> <warm_white>\n");
        return 1;
    }

    uint8_t values[5];
    for (int i = 0; i < 5; i++) {
        if (parse_percent_arg(argv[i + 1], &values[i]) != 0) {
            printf("Invalid channel value at arg %d (0-100)\n", i + 1);
            return 1;
        }
    }

    esp_err_t err = fishduino_fluval_set_channels(values[0], values[1], values[2], values[3], values[4]);
    if (err != ESP_OK) {
        printf("fluval_set failed: %s\n", esp_err_to_name(err));
        return 1;
    }
    printf("Fluval channel set queued\n");
    return 0;
}

void fishduino_fluval_console_register(void)
{
    const esp_console_cmd_t fluval_cmd = {
        .command = "fluval",
        .help = "Print Fluval Plant 4.0 state",
        .func = &cmd_fluval_status,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&fluval_cmd));

    const esp_console_cmd_t read_cmd = {
        .command = "fluval_read",
        .help = "Request Fluval status read",
        .func = &cmd_fluval_read,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&read_cmd));

    const esp_console_cmd_t manual_cmd = {
        .command = "fluval_manual",
        .help = "Set Fluval Manual mode",
        .func = &cmd_fluval_manual,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&manual_cmd));

    const esp_console_cmd_t auto_cmd = {
        .command = "fluval_auto",
        .help = "Set Fluval Auto mode",
        .func = &cmd_fluval_auto,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&auto_cmd));

    const esp_console_cmd_t setall_cmd = {
        .command = "fluval_setall",
        .help = "Set all Fluval channels to same percent",
        .func = &cmd_fluval_setall,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&setall_cmd));

    const esp_console_cmd_t set_cmd = {
        .command = "fluval_set",
        .help = "Set Fluval channels: pink blue cold_white white warm_white",
        .func = &cmd_fluval_set,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&set_cmd));
}

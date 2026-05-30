#include "uart_console.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "fluval_ble.h"

static const char *TAG = "uart_console";

#define LINE_MAX 192

static void print_help(void)
{
    printf("Commands:\n");
    printf("  HELP\n");
    printf("  FLUVAL READ\n");
    printf("  FLUVAL STATUS\n");
    printf("  FLUVAL MODE MANUAL\n");
    printf("  FLUVAL MODE AUTO\n");
    printf("  FLUVAL SETALL <0-100>\n");
    printf("  FLUVAL SET <pink> <blue> <cold_white> <white> <warm_white>\n");
}

void fluval_print_state_line(const fluval_state_t *st)
{
    if (st == NULL) {
        return;
    }

    printf("FLUVAL STATE %s %u %u %u %u %u AVG %u RSSI %d\n", fluval_mode_to_string(st->mode),
           (unsigned)st->pink, (unsigned)st->blue, (unsigned)st->cold_white, (unsigned)st->white,
           (unsigned)st->warm_white, (unsigned)st->avg_output, st->rssi);
}

static void print_error_disconnected(void)
{
    printf("FLUVAL ERROR DISCONNECTED\n");
}

static void print_error_bad_args(void)
{
    printf("FLUVAL ERROR BAD_ARGS\n");
}

static void print_error_timeout(void)
{
    printf("FLUVAL ERROR TIMEOUT\n");
}

static void print_error_unknown_cmd(void)
{
    printf("FLUVAL ERROR UNKNOWN_CMD\n");
}

static void print_ok(void)
{
    printf("FLUVAL OK\n");
}

static esp_err_t map_ble_err(esp_err_t err)
{
    if (err == ESP_ERR_INVALID_STATE) {
        print_error_disconnected();
        return err;
    }
    if (err == ESP_ERR_TIMEOUT) {
        print_error_timeout();
        return err;
    }
    if (err != ESP_OK) {
        print_error_timeout();
        return err;
    }
    return ESP_OK;
}

static int parse_percent(const char *text, uint8_t *out)
{
    if (text == NULL || out == NULL) {
        return -1;
    }
    char *end = NULL;
    long v = strtol(text, &end, 10);
    if (end == text || *end != '\0' || v < 0 || v > 100) {
        return -1;
    }
    *out = (uint8_t)v;
    return 0;
}

static void uppercase_inplace(char *s)
{
    if (s == NULL) {
        return;
    }
    for (; *s != '\0'; s++) {
        *s = (char)toupper((unsigned char)*s);
    }
}

static void trim_inplace(char *s)
{
    if (s == NULL) {
        return;
    }

    size_t len = strlen(s);
    while (len > 0 && isspace((unsigned char)s[len - 1])) {
        s[--len] = '\0';
    }

    size_t start = 0;
    while (s[start] != '\0' && isspace((unsigned char)s[start])) {
        start++;
    }
    if (start > 0) {
        memmove(s, s + start, strlen(s + start) + 1);
    }
}

static void handle_line(char *line)
{
    trim_inplace(line);
    if (line[0] == '\0') {
        return;
    }

    uppercase_inplace(line);
    ESP_LOGI(TAG, "cmd: %s", line);

    if (strcmp(line, "HELP") == 0) {
        print_help();
        return;
    }

    if (strcmp(line, "FLUVAL STATUS") == 0) {
        fluval_state_t st;
        if (fluval_ble_get_state(&st) != ESP_OK) {
            print_error_disconnected();
            return;
        }
        fluval_print_state_line(&st);
        return;
    }

    if (strcmp(line, "FLUVAL READ") == 0) {
        esp_err_t err = fluval_ble_wait_status(NULL, FLUVAL_CMD_WAIT_MS);
        if (map_ble_err(err) != ESP_OK) {
            return;
        }
        fluval_state_t st;
        if (fluval_ble_get_state(&st) == ESP_OK) {
            fluval_print_state_line(&st);
        }
        return;
    }

    if (strcmp(line, "FLUVAL MODE MANUAL") == 0) {
        esp_err_t err = fluval_ble_set_mode_manual();
        if (map_ble_err(err) != ESP_OK) {
            return;
        }
        print_ok();
        fluval_state_t st;
        if (fluval_ble_get_state(&st) == ESP_OK) {
            fluval_print_state_line(&st);
        }
        return;
    }

    if (strcmp(line, "FLUVAL MODE AUTO") == 0) {
        esp_err_t err = fluval_ble_set_mode_auto();
        if (map_ble_err(err) != ESP_OK) {
            return;
        }
        print_ok();
        fluval_state_t st;
        if (fluval_ble_get_state(&st) == ESP_OK) {
            fluval_print_state_line(&st);
        }
        return;
    }

    if (strncmp(line, "FLUVAL SETALL ", 14) == 0) {
        uint8_t pct = 0;
        if (parse_percent(line + 14, &pct) != 0) {
            print_error_bad_args();
            return;
        }
        esp_err_t err = fluval_ble_set_all(pct);
        if (map_ble_err(err) != ESP_OK) {
            return;
        }
        print_ok();
        fluval_state_t st;
        if (fluval_ble_get_state(&st) == ESP_OK) {
            fluval_print_state_line(&st);
        }
        return;
    }

    if (strncmp(line, "FLUVAL SET ", 11) == 0) {
        unsigned p = 0;
        unsigned b = 0;
        unsigned cw = 0;
        unsigned w = 0;
        unsigned ww = 0;
        int matched = sscanf(line + 11, "%u %u %u %u %u", &p, &b, &cw, &w, &ww);
        if (matched != 5 || p > 100 || b > 100 || cw > 100 || w > 100 || ww > 100) {
            print_error_bad_args();
            return;
        }
        esp_err_t err = fluval_ble_set_channels((uint8_t)p, (uint8_t)b, (uint8_t)cw, (uint8_t)w, (uint8_t)ww);
        if (map_ble_err(err) != ESP_OK) {
            return;
        }
        print_ok();
        fluval_state_t st;
        if (fluval_ble_get_state(&st) == ESP_OK) {
            fluval_print_state_line(&st);
        }
        return;
    }

    print_error_unknown_cmd();
}

static void uart_cmd_task(void *arg)
{
    (void)arg;
    char line[LINE_MAX];
    size_t len = 0;

    while (true) {
        int ch = getchar();
        if (ch == EOF) {
            vTaskDelay(pdMS_TO_TICKS(20));
            continue;
        }

        if (ch == '\r') {
            continue;
        }

        if (ch == '\n') {
            line[len] = '\0';
            handle_line(line);
            len = 0;
            continue;
        }

        if (len + 1 >= sizeof(line)) {
            len = 0;
            continue;
        }

        line[len++] = (char)ch;
    }
}

void uart_console_start(void)
{
    setvbuf(stdin, NULL, _IONBF, 0);
    setvbuf(stdout, NULL, _IONBF, 0);

    BaseType_t ok = xTaskCreate(uart_cmd_task, "uart_cmd", 4096, NULL, 5, NULL);
    if (ok != pdPASS) {
        ESP_LOGE(TAG, "failed to start uart command task");
    }
}

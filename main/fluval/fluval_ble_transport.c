#include "fluval_transport.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_log.h"
#include "fluval_ble_client.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "fluval_ble_tr";

typedef struct {
    bool configured;
    bool started;
    fishduino_fluval_transport_line_cb_t line_cb;
    void *line_ctx;
    TaskHandle_t pump_task;
    uint32_t last_emit_ms;
} fluval_ble_transport_ctx_t;

static fluval_ble_transport_ctx_t s_ble_tr;

esp_err_t fishduino_fluval_ble_transport_set_callback(fishduino_fluval_transport_line_cb_t cb, void *ctx)
{
    s_ble_tr.line_cb = cb;
    s_ble_tr.line_ctx = ctx;
    return ESP_OK;
}

static void dispatch_line(const char *line)
{
    if (line == NULL || line[0] == '\0' || s_ble_tr.line_cb == NULL) {
        return;
    }
    s_ble_tr.line_cb(line, s_ble_tr.line_ctx);
}

static void emit_state_line(const fishduino_fluval_ble_client_state_t *st)
{
    if (st == NULL || !st->protocol.valid) {
        return;
    }

    char line[160];
    snprintf(line, sizeof(line), "FLUVAL STATE %s %u %u %u %u %u AVG %u RSSI %d",
             fluval_protocol_mode_token(st->protocol.mode), (unsigned)st->protocol.pink,
             (unsigned)st->protocol.blue, (unsigned)st->protocol.cold_white, (unsigned)st->protocol.white,
             (unsigned)st->protocol.warm_white, (unsigned)st->protocol.avg_output, st->rssi);
    dispatch_line(line);
}

static void emit_ok(void)
{
    dispatch_line("FLUVAL OK");
}

static void emit_error(const char *reason)
{
    char line[64];
    snprintf(line, sizeof(line), "FLUVAL ERROR %s", reason != NULL ? reason : "UNKNOWN");
    dispatch_line(line);
}

static esp_err_t map_ble_err(esp_err_t err)
{
    if (err == ESP_ERR_INVALID_STATE) {
        emit_error("DISCONNECTED");
        return err;
    }
    if (err == ESP_ERR_TIMEOUT) {
        emit_error("TIMEOUT");
        return err;
    }
    if (err != ESP_OK) {
        emit_error("TIMEOUT");
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

static esp_err_t handle_command_line(char *line)
{
    uppercase_inplace(line);
    ESP_LOGD(TAG, "cmd: %s", line);

    if (strcmp(line, "FLUVAL READ") == 0) {
        fishduino_fluval_ble_client_state_t st;
        esp_err_t err = fishduino_fluval_ble_client_wait_status(&st, FLUVAL_BLE_CMD_WAIT_MS);
        if (map_ble_err(err) != ESP_OK) {
            return err;
        }
        emit_state_line(&st);
        return ESP_OK;
    }

    if (strcmp(line, "FLUVAL MODE MANUAL") == 0) {
        esp_err_t err = fishduino_fluval_ble_client_set_mode_manual();
        if (map_ble_err(err) != ESP_OK) {
            return err;
        }
        emit_ok();
        fishduino_fluval_ble_client_state_t st;
        if (fishduino_fluval_ble_client_get_state(&st) == ESP_OK) {
            emit_state_line(&st);
        }
        return ESP_OK;
    }

    if (strcmp(line, "FLUVAL MODE AUTO") == 0) {
        esp_err_t err = fishduino_fluval_ble_client_set_mode_auto();
        if (map_ble_err(err) != ESP_OK) {
            return err;
        }
        emit_ok();
        fishduino_fluval_ble_client_state_t st;
        if (fishduino_fluval_ble_client_get_state(&st) == ESP_OK) {
            emit_state_line(&st);
        }
        return ESP_OK;
    }

    if (strncmp(line, "FLUVAL SETALL ", 14) == 0) {
        uint8_t percent = 0;
        if (parse_percent(line + 14, &percent) != 0) {
            emit_error("BAD_ARGS");
            return ESP_ERR_INVALID_ARG;
        }
        esp_err_t err = fishduino_fluval_ble_client_set_all(percent);
        if (map_ble_err(err) != ESP_OK) {
            return err;
        }
        emit_ok();
        fishduino_fluval_ble_client_state_t st;
        if (fishduino_fluval_ble_client_get_state(&st) == ESP_OK) {
            emit_state_line(&st);
        }
        return ESP_OK;
    }

    if (strncmp(line, "FLUVAL SET ", 11) == 0) {
        unsigned pink = 0;
        unsigned blue = 0;
        unsigned cold_white = 0;
        unsigned white = 0;
        unsigned warm_white = 0;
        int matched = sscanf(line + 11, "%u %u %u %u %u", &pink, &blue, &cold_white, &white, &warm_white);
        if (matched != 5 || pink > 100 || blue > 100 || cold_white > 100 || white > 100 || warm_white > 100) {
            emit_error("BAD_ARGS");
            return ESP_ERR_INVALID_ARG;
        }
        esp_err_t err = fishduino_fluval_ble_client_set_channels((uint8_t)pink, (uint8_t)blue, (uint8_t)cold_white,
                                                                 (uint8_t)white, (uint8_t)warm_white);
        if (map_ble_err(err) != ESP_OK) {
            return err;
        }
        emit_ok();
        fishduino_fluval_ble_client_state_t st;
        if (fishduino_fluval_ble_client_get_state(&st) == ESP_OK) {
            emit_state_line(&st);
        }
        return ESP_OK;
    }

    emit_error("UNKNOWN_CMD");
    return ESP_ERR_NOT_SUPPORTED;
}

static void pump_task(void *arg)
{
    (void)arg;
    uint32_t last_seen_update = 0;

    while (s_ble_tr.started) {
        fishduino_fluval_ble_client_state_t st;
        if (fishduino_fluval_ble_client_get_state(&st) == ESP_OK && st.protocol.valid &&
            st.last_update_ms != 0 && st.last_update_ms != last_seen_update) {
            last_seen_update = st.last_update_ms;
            emit_state_line(&st);
        }
        vTaskDelay(pdMS_TO_TICKS(250));
    }

    s_ble_tr.pump_task = NULL;
    vTaskDelete(NULL);
}

static esp_err_t ble_transport_init(void *ctx)
{
    (void)ctx;
    s_ble_tr.configured = true;
    return ESP_OK;
}

static esp_err_t ble_transport_start(void *ctx)
{
    (void)ctx;
    if (!s_ble_tr.configured || s_ble_tr.started) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t err = fishduino_fluval_ble_client_init();
    if (err != ESP_OK) {
        return err;
    }
    err = fishduino_fluval_ble_client_start();
    if (err != ESP_OK) {
        return err;
    }

    s_ble_tr.started = true;
    BaseType_t ok = xTaskCreate(pump_task, "fluval_ble_tr", 3072, NULL, 4, &s_ble_tr.pump_task);
    if (ok != pdPASS) {
        s_ble_tr.started = false;
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

static esp_err_t ble_transport_stop(void *ctx)
{
    (void)ctx;
    s_ble_tr.started = false;
    while (s_ble_tr.pump_task != NULL) {
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    return ESP_OK;
}

static esp_err_t ble_transport_send_line(void *ctx, const char *line)
{
    (void)ctx;
    if (!s_ble_tr.started || line == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    char copy[192];
    strncpy(copy, line, sizeof(copy) - 1);
    copy[sizeof(copy) - 1] = '\0';
    return handle_command_line(copy);
}

static bool ble_transport_is_active(void *ctx)
{
    (void)ctx;
    return s_ble_tr.started && fishduino_fluval_ble_client_is_connected();
}

static const fishduino_fluval_transport_ops_t s_ble_ops = {
    .init = ble_transport_init,
    .start = ble_transport_start,
    .stop = ble_transport_stop,
    .send_line = ble_transport_send_line,
    .is_active = ble_transport_is_active,
};

const fishduino_fluval_transport_ops_t *fishduino_fluval_ble_transport_ops(void)
{
    return &s_ble_ops;
}

esp_err_t fishduino_fluval_ble_transport_apply_config(const char *target_name, uint16_t poll_interval_s,
                                                      uint16_t stale_timeout_s)
{
    fishduino_fluval_ble_client_config_t cfg = {0};
    if (target_name != NULL) {
        strncpy(cfg.target_name, target_name, sizeof(cfg.target_name) - 1);
    }
    cfg.poll_interval_ms = poll_interval_s > 0 ? (uint32_t)poll_interval_s * 1000U : 10000;
    cfg.stale_timeout_ms = stale_timeout_s > 0 ? (uint32_t)stale_timeout_s * 1000U : 30000;
    return fishduino_fluval_ble_client_set_config(&cfg);
}

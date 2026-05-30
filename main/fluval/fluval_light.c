#include "fluval_light.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "esp_log.h"
#include "fluval_protocol.h"
#include "fluval_transport.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "hardware_pins.h"
#include "storage/settings_nvs.h"
#include "storage/settings_runtime.h"

static const char *TAG = "fluval";

typedef enum {
    FLUVAL_CMD_READ = 1,
    FLUVAL_CMD_MODE_MANUAL,
    FLUVAL_CMD_MODE_AUTO,
    FLUVAL_CMD_SET_CHANNELS,
    FLUVAL_CMD_SET_ALL,
} fluval_cmd_type_t;

typedef struct {
    fluval_cmd_type_t type;
    uint8_t pink;
    uint8_t blue;
    uint8_t cold_white;
    uint8_t white;
    uint8_t warm_white;
} fluval_cmd_t;

static fishduino_fluval_state_t s_state;
static SemaphoreHandle_t s_state_mutex;
static QueueHandle_t s_cmd_queue;
static fishduino_fluval_transport_t s_transport;
static bool s_module_ready;
static bool s_task_running;
static uint32_t s_last_poll_ms;
static bool s_transport_usable;
static fishduino_fluval_settings_t s_cfg;

static uint32_t now_ms(void)
{
    return (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
}

static void state_lock(void)
{
    if (s_state_mutex != NULL) {
        xSemaphoreTake(s_state_mutex, portMAX_DELAY);
    }
}

static void state_unlock(void)
{
    if (s_state_mutex != NULL) {
        xSemaphoreGive(s_state_mutex);
    }
}

static bool uart_hw_available(void)
{
    return FISHDUINO_FLUVAL_UART_NUM >= 0 && FISHDUINO_FLUVAL_UART_TX >= 0 && FISHDUINO_FLUVAL_UART_RX >= 0;
}

static void refresh_config(void)
{
    fishduino_settings_t settings;
    if (fishduino_settings_get_snapshot(&settings)) {
        s_cfg = settings.fluval;
    }
}

static bool integration_enabled(void)
{
    return s_cfg.enabled && uart_hw_available();
}

static fishduino_fluval_mode_t mode_from_token(const char *token)
{
    if (token == NULL) {
        return FISHDUINO_FLUVAL_MODE_UNKNOWN;
    }
    if (strcasecmp(token, "MANUAL") == 0) {
        return FISHDUINO_FLUVAL_MODE_MANUAL;
    }
    if (strcasecmp(token, "AUTO") == 0) {
        return FISHDUINO_FLUVAL_MODE_AUTO;
    }
    return FISHDUINO_FLUVAL_MODE_UNKNOWN;
}

static void apply_state_update(fishduino_fluval_mode_t mode, uint8_t pink, uint8_t blue, uint8_t cold_white,
                               uint8_t white, uint8_t warm_white, uint8_t avg, int rssi, bool connected)
{
    state_lock();
    s_state.connected = connected;
    s_state.stale = false;
    s_state.mode = mode;
    s_state.pink = pink;
    s_state.blue = blue;
    s_state.cold_white = cold_white;
    s_state.white = white;
    s_state.warm_white = warm_white;
    s_state.avg_output = avg;
    if (rssi != INT32_MIN) {
        s_state.rssi = rssi;
    }
    s_state.last_update_ms = now_ms();
    state_unlock();
}

static void mark_transport_error(void)
{
    state_lock();
    s_state.connected = false;
    state_unlock();
}

static bool parse_state_line(const char *line)
{
    if (line == NULL || strncmp(line, "FLUVAL STATE ", 13) != 0) {
        return false;
    }

    char mode_token[16] = {0};
    unsigned pink = 0;
    unsigned blue = 0;
    unsigned cold_white = 0;
    unsigned white = 0;
    unsigned warm_white = 0;
    unsigned avg = 0;
    int rssi = INT32_MIN;

    int matched = sscanf(line + 13, "%15s %u %u %u %u %u AVG %u RSSI %d", mode_token, &pink, &blue, &cold_white,
                         &white, &warm_white, &avg, &rssi);
    if (matched < 7) {
        return false;
    }

    apply_state_update(mode_from_token(mode_token), fluval_protocol_clamp_percent((int)pink),
                       fluval_protocol_clamp_percent((int)blue), fluval_protocol_clamp_percent((int)cold_white),
                       fluval_protocol_clamp_percent((int)white), fluval_protocol_clamp_percent((int)warm_white),
                       fluval_protocol_clamp_percent((int)avg), rssi, true);
    return true;
}

static void on_transport_line(const char *line, void *ctx)
{
    (void)ctx;
    if (line == NULL) {
        return;
    }

    if (parse_state_line(line)) {
        return;
    }

    if (strncmp(line, "FLUVAL OK", 9) == 0) {
        state_lock();
        s_state.connected = true;
        s_state.stale = false;
        state_unlock();
        return;
    }

    if (strncmp(line, "FLUVAL ERROR", 12) == 0) {
        ESP_LOGW(TAG, "Helper error: %s", line);
        mark_transport_error();
    }
}

static esp_err_t send_transport_command(const char *line)
{
    if (!integration_enabled() || !s_transport_usable) {
        return ESP_ERR_INVALID_STATE;
    }
    return fishduino_fluval_transport_send_line(&s_transport, line);
}

static esp_err_t queue_command(const fluval_cmd_t *cmd)
{
    if (cmd == NULL || s_cmd_queue == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    if (!integration_enabled()) {
        return ESP_ERR_INVALID_STATE;
    }
    if (xQueueSend(s_cmd_queue, cmd, 0) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    return ESP_OK;
}

static void handle_command(const fluval_cmd_t *cmd)
{
    char line[96];
    esp_err_t err = ESP_ERR_INVALID_STATE;

    switch (cmd->type) {
    case FLUVAL_CMD_READ:
        err = send_transport_command("FLUVAL READ");
        break;
    case FLUVAL_CMD_MODE_MANUAL:
        err = send_transport_command("FLUVAL MODE MANUAL");
        break;
    case FLUVAL_CMD_MODE_AUTO:
        err = send_transport_command("FLUVAL MODE AUTO");
        break;
    case FLUVAL_CMD_SET_ALL:
        snprintf(line, sizeof(line), "FLUVAL SETALL %u", (unsigned)fluval_protocol_clamp_percent(cmd->pink));
        err = send_transport_command(line);
        break;
    case FLUVAL_CMD_SET_CHANNELS:
        snprintf(line, sizeof(line), "FLUVAL SET %u %u %u %u %u", (unsigned)cmd->pink, (unsigned)cmd->blue,
                 (unsigned)cmd->cold_white, (unsigned)cmd->white, (unsigned)cmd->warm_white);
        err = send_transport_command(line);
        break;
    default:
        break;
    }

    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Transport send failed: %s", esp_err_to_name(err));
        mark_transport_error();
    }
}

static void fluval_task(void *arg)
{
    (void)arg;
    fluval_cmd_t cmd;

    while (s_task_running) {
        refresh_config();

        if (!integration_enabled()) {
            s_transport_usable = false;
            vTaskDelay(pdMS_TO_TICKS(500));
            continue;
        }

        if (!s_transport_usable) {
            fishduino_fluval_transport_stop(&s_transport);
            s_transport.ops = fishduino_fluval_uart_transport_ops();
            s_transport.ctx = NULL;
            if (fishduino_fluval_transport_init(&s_transport, on_transport_line, NULL) == ESP_OK &&
                fishduino_fluval_transport_start(&s_transport) == ESP_OK) {
                s_transport_usable = fishduino_fluval_transport_is_active(&s_transport);
            }
        }

        if (xQueueReceive(s_cmd_queue, &cmd, pdMS_TO_TICKS(200)) == pdTRUE) {
            handle_command(&cmd);
        }
    }

    fishduino_fluval_transport_stop(&s_transport);
    vTaskDelete(NULL);
}

static void update_stale_flag(void)
{
    if (!integration_enabled()) {
        state_lock();
        s_state.stale = false;
        s_state.connected = false;
        state_unlock();
        return;
    }

    state_lock();
    if (s_state.last_update_ms == 0) {
        s_state.stale = true;
    } else {
        uint32_t age_ms = now_ms() - s_state.last_update_ms;
        s_state.stale = age_ms > ((uint32_t)s_cfg.stale_timeout_s * 1000U);
        if (s_state.stale) {
            s_state.connected = false;
        }
    }
    state_unlock();
}

const char *fishduino_fluval_mode_text(fishduino_fluval_mode_t mode)
{
    switch (mode) {
    case FISHDUINO_FLUVAL_MODE_MANUAL:
        return "Manual";
    case FISHDUINO_FLUVAL_MODE_AUTO:
        return "Auto";
    default:
        return "--";
    }
}

const char *fishduino_fluval_link_text(fishduino_fluval_link_t link)
{
    switch (link) {
    case FISHDUINO_FLUVAL_LINK_OK:
        return "OK";
    case FISHDUINO_FLUVAL_LINK_STALE:
        return "STALE";
    case FISHDUINO_FLUVAL_LINK_OFFLINE:
        return "OFFLINE";
    default:
        return "DISABLED";
    }
}

fishduino_fluval_link_t fishduino_fluval_get_link_status(void)
{
    refresh_config();
    if (!integration_enabled()) {
        return FISHDUINO_FLUVAL_LINK_DISABLED;
    }

    fishduino_fluval_state_t snap;
    if (!fishduino_fluval_get_state(&snap)) {
        return FISHDUINO_FLUVAL_LINK_OFFLINE;
    }
    if (snap.stale) {
        return FISHDUINO_FLUVAL_LINK_STALE;
    }
    if (!snap.connected) {
        return FISHDUINO_FLUVAL_LINK_OFFLINE;
    }
    return FISHDUINO_FLUVAL_LINK_OK;
}

esp_err_t fishduino_fluval_init(void)
{
    memset(&s_state, 0, sizeof(s_state));
    s_state.rssi = 0;
    s_module_ready = false;

    if (!fluval_protocol_run_selftests()) {
        ESP_LOGW(TAG, "Fluval protocol selftests failed");
    } else {
        ESP_LOGI(TAG, "Fluval protocol selftests passed");
    }

    s_state_mutex = xSemaphoreCreateMutex();
    s_cmd_queue = xQueueCreate(8, sizeof(fluval_cmd_t));
    if (s_state_mutex == NULL || s_cmd_queue == NULL) {
        return ESP_ERR_NO_MEM;
    }

    refresh_config();
    s_transport.ops = fishduino_fluval_stub_transport_ops();
    s_transport.ctx = NULL;
    fishduino_fluval_transport_init(&s_transport, on_transport_line, NULL);

    s_module_ready = true;
    return ESP_OK;
}

esp_err_t fishduino_fluval_start(void)
{
    if (!s_module_ready || s_task_running) {
        return ESP_ERR_INVALID_STATE;
    }

    s_task_running = true;
    BaseType_t ok = xTaskCreate(fluval_task, "fluval", 4096, NULL, 4, NULL);
    if (ok != pdPASS) {
        s_task_running = false;
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

esp_err_t fishduino_fluval_tick(void)
{
    if (!s_module_ready) {
        return ESP_ERR_INVALID_STATE;
    }

    refresh_config();
    update_stale_flag();

    if (!integration_enabled()) {
        return ESP_OK;
    }

    uint32_t t = now_ms();
    uint32_t interval_ms = (uint32_t)s_cfg.poll_interval_s * 1000U;
    if (interval_ms == 0) {
        interval_ms = 10000;
    }

    if (s_last_poll_ms == 0 || (t - s_last_poll_ms) >= interval_ms) {
        s_last_poll_ms = t;
        fishduino_fluval_request_status();
    }

    return ESP_OK;
}

esp_err_t fishduino_fluval_get_state(fishduino_fluval_state_t *out)
{
    if (out == NULL || s_state_mutex == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (xSemaphoreTake(s_state_mutex, pdMS_TO_TICKS(200)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    *out = s_state;
    xSemaphoreGive(s_state_mutex);
    return ESP_OK;
}

esp_err_t fishduino_fluval_request_status(void)
{
    fluval_cmd_t cmd = {.type = FLUVAL_CMD_READ};
    return queue_command(&cmd);
}

esp_err_t fishduino_fluval_set_mode_manual(void)
{
    fluval_cmd_t cmd = {.type = FLUVAL_CMD_MODE_MANUAL};
    return queue_command(&cmd);
}

esp_err_t fishduino_fluval_set_mode_auto(void)
{
    fluval_cmd_t cmd = {.type = FLUVAL_CMD_MODE_AUTO};
    return queue_command(&cmd);
}

esp_err_t fishduino_fluval_set_channels(uint8_t pink, uint8_t blue, uint8_t cold_white, uint8_t white,
                                        uint8_t warm_white)
{
    fluval_cmd_t cmd = {
        .type = FLUVAL_CMD_SET_CHANNELS,
        .pink = fluval_protocol_clamp_percent(pink),
        .blue = fluval_protocol_clamp_percent(blue),
        .cold_white = fluval_protocol_clamp_percent(cold_white),
        .white = fluval_protocol_clamp_percent(white),
        .warm_white = fluval_protocol_clamp_percent(warm_white),
    };
    return queue_command(&cmd);
}

esp_err_t fishduino_fluval_set_all(uint8_t percent)
{
    uint8_t p = fluval_protocol_clamp_percent(percent);
    fluval_cmd_t cmd = {
        .type = FLUVAL_CMD_SET_ALL,
        .pink = p,
        .blue = p,
        .cold_white = p,
        .white = p,
        .warm_white = p,
    };
    return queue_command(&cmd);
}

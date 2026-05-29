/*
 * Shelly Gen2+/Gen4 local RPC (HTTP GET):
 *   GET http://<ip>/rpc/Shelly.GetStatus
 *   GET http://<ip>/rpc/Switch.GetStatus?id=0
 *   GET http://<ip>/rpc/Switch.Set?id=0&on=true
 *   GET http://<ip>/rpc/Switch.Set?id=0&on=false
 *
 * Must run only from shelly_task — blocks up to FISHDUINO_SHELLY_HTTP_TIMEOUT_MS.
 */

#include "shelly_client.h"

#include <stdio.h>
#include <string.h>

#include "cJSON.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "shelly/shelly_config.h"

static const char *TAG = "shelly_client";

#define HTTP_BUF_SIZE 2048

static bool s_logged_filter_energy;

static void error_text_set(fishduino_shelly_switch_status_t *out, const char *msg)
{
    if (out == NULL || msg == NULL) {
        return;
    }
    snprintf(out->error_text, sizeof(out->error_text), "%s", msg);
    out->error_text[sizeof(out->error_text) - 1] = '\0';
}

static float json_get_float(const cJSON *obj, const char *key, float def)
{
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (cJSON_IsNumber(item)) {
        return (float)item->valuedouble;
    }
    return def;
}

static bool json_get_bool(const cJSON *obj, const char *key, bool def)
{
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (cJSON_IsBool(item)) {
        return cJSON_IsTrue(item);
    }
    return def;
}

static bool json_has_switch_fields(const cJSON *root)
{
    const cJSON *id = cJSON_GetObjectItemCaseSensitive(root, "id");
    const cJSON *output = cJSON_GetObjectItemCaseSensitive(root, "output");
    if (cJSON_IsNumber(id) || cJSON_IsBool(output)) {
        return true;
    }
    return false;
}

static void parse_temperature(const cJSON *root, fishduino_shelly_switch_status_t *out)
{
    const cJSON *temp = cJSON_GetObjectItemCaseSensitive(root, "temperature");
    if (cJSON_IsObject(temp)) {
        const cJSON *tc = cJSON_GetObjectItemCaseSensitive(temp, "tC");
        if (cJSON_IsNumber(tc)) {
            float t_c = (float)tc->valuedouble;
            out->temperature_f = t_c * 9.0f / 5.0f + 32.0f;
        }
    } else if (cJSON_IsNumber(temp)) {
        float t_c = (float)temp->valuedouble;
        out->temperature_f = t_c * 9.0f / 5.0f + 32.0f;
    }
}

static bool parse_switch_json(const char *body, fishduino_shelly_switch_status_t *out)
{
    cJSON *root = cJSON_Parse(body);
    if (root == NULL) {
        return false;
    }

    if (!json_has_switch_fields(root)) {
        const cJSON *code = cJSON_GetObjectItemCaseSensitive(root, "code");
        const cJSON *message = cJSON_GetObjectItemCaseSensitive(root, "message");
        if (cJSON_IsNumber(code) && cJSON_IsString(message) && message->valuestring != NULL) {
            char rpc_err[80];
            snprintf(rpc_err, sizeof(rpc_err), "RPC %d: %s", code->valueint, message->valuestring);
            error_text_set(out, rpc_err);
        }
        cJSON_Delete(root);
        return false;
    }

    out->output = json_get_bool(root, "output", out->output);
    out->watts = json_get_float(root, "apower", out->watts);
    out->voltage = json_get_float(root, "voltage", out->voltage);
    out->current = json_get_float(root, "current", out->current);
    out->power_factor = json_get_float(root, "pf", out->power_factor);
    out->frequency = json_get_float(root, "freq", out->frequency);

    const cJSON *aenergy = cJSON_GetObjectItemCaseSensitive(root, "aenergy");
    if (cJSON_IsObject(aenergy)) {
        out->energy_wh = json_get_float(aenergy, "total", out->energy_wh);
    }

    parse_temperature(root, out);

    const cJSON *errors = cJSON_GetObjectItemCaseSensitive(root, "errors");
    if (cJSON_IsArray(errors) && cJSON_GetArraySize(errors) > 0) {
        const cJSON *e0 = cJSON_GetArrayItem(errors, 0);
        if (cJSON_IsString(e0) && e0->valuestring != NULL) {
            error_text_set(out, e0->valuestring);
        }
    }

    cJSON_Delete(root);
    return true;
}

typedef struct {
    char body[HTTP_BUF_SIZE];
    bool truncated;
} http_body_ctx_t;

static esp_err_t http_event(esp_http_client_event_t *evt)
{
    if (evt->event_id != HTTP_EVENT_ON_DATA || evt->user_data == NULL) {
        return ESP_OK;
    }

    http_body_ctx_t *ctx = (http_body_ctx_t *)evt->user_data;
    char *buf = ctx->body;
    size_t cap = HTTP_BUF_SIZE - 1;
    size_t cur = strnlen(buf, cap);
    if (cur >= cap || evt->data_len == 0) {
        return ESP_OK;
    }

    size_t copy = evt->data_len;
    if (cur + copy > cap) {
        copy = cap - cur;
        ctx->truncated = true;
    }
    memcpy(buf + cur, evt->data, copy);
    buf[cur + copy] = '\0';
    return ESP_OK;
}

typedef struct {
    char body[HTTP_BUF_SIZE];
    bool truncated;
    int status_code;
    bool ok;
} http_result_t;

static bool http_get(const char *url, http_result_t *result)
{
    result->body[0] = '\0';
    result->truncated = false;
    result->status_code = 0;
    result->ok = false;

    http_body_ctx_t body_ctx = {0};

    esp_http_client_config_t cfg = {
        .url = url,
        .method = HTTP_METHOD_GET,
        .timeout_ms = FISHDUINO_SHELLY_HTTP_TIMEOUT_MS,
        .event_handler = http_event,
        .user_data = &body_ctx,
        .buffer_size = 512,
        .buffer_size_tx = 512,
    };

    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (client == NULL) {
        return false;
    }

    esp_err_t err = esp_http_client_perform(client);
    result->status_code = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);

    memcpy(result->body, body_ctx.body, sizeof(result->body));
    result->body[sizeof(result->body) - 1] = '\0';
    result->truncated = body_ctx.truncated;

    if (result->truncated) {
        ESP_LOGW(TAG, "HTTP response truncated (buffer %d bytes): %s", HTTP_BUF_SIZE, url);
    }

    result->ok = (err == ESP_OK && result->status_code >= 200 && result->status_code < 300 &&
                  result->body[0] != '\0' && !result->truncated);
    return result->ok;
}

static void mark_failure(fishduino_shelly_switch_status_t *out)
{
    out->fail_count++;
    if (out->fail_count >= FISHDUINO_SHELLY_FAIL_OFFLINE) {
        out->online = false;
    }
}

static void mark_success(fishduino_shelly_switch_status_t *out)
{
    out->fail_count = 0;
    out->online = true;
    out->last_success_ms = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
}

static void handle_http_error(int status, fishduino_shelly_switch_status_t *out)
{
    if (status == 401) {
        error_text_set(out, "HTTP 401 auth required");
        ESP_LOGW(TAG, "Shelly returned 401 — disable LAN authentication on the plug");
    }
}

bool fishduino_shelly_get_switch_status(const char *ip, int switch_id, fishduino_shelly_switch_status_t *out)
{
    if (ip == NULL || out == NULL || ip[0] == '\0') {
        return false;
    }

    char url[128];
    snprintf(url, sizeof(url), "http://%s/rpc/Switch.GetStatus?id=%d", ip, switch_id);

    http_result_t hr;
    if (!http_get(url, &hr)) {
        ESP_LOGW(TAG, "GET failed: %s status=%d truncated=%d", url, hr.status_code, (int)hr.truncated);
        handle_http_error(hr.status_code, out);
        mark_failure(out);
        return false;
    }

    if (!parse_switch_json(hr.body, out)) {
        ESP_LOGW(TAG, "Parse failed: %s", url);
        mark_failure(out);
        return false;
    }

    mark_success(out);

    if (!s_logged_filter_energy && out->energy_wh > 0.0f) {
        s_logged_filter_energy = true;
        ESP_LOGI(TAG, "Sample aenergy.total=%.3f (displayed as kWh = value/1000 if Wh)", (double)out->energy_wh);
    }

    ESP_LOGD(TAG, "%s out=%d %.1fW online=%d", ip, out->output, (double)out->watts, out->online);
    return true;
}

static bool shelly_set_switch_output(const char *ip, int switch_id, bool on)
{
    if (ip == NULL || ip[0] == '\0') {
        return false;
    }

    char url[128];
    snprintf(url, sizeof(url), "http://%s/rpc/Switch.Set?id=%d&on=%s", ip, switch_id,
             on ? "true" : "false");

    http_result_t hr;
    if (!http_get(url, &hr)) {
        ESP_LOGW(TAG, "SET failed: %s status=%d truncated=%d", url, hr.status_code, (int)hr.truncated);
        if (hr.status_code == 401) {
            ESP_LOGW(TAG, "Shelly auth enabled — disable authentication on CO2 plug");
        }
        return false;
    }

    ESP_LOGI(TAG, "Switch.Set %s id=%d on=%d", ip, switch_id, on);
    return true;
}

bool fishduino_shelly_co2_set_output(const fishduino_settings_t *settings, bool on)
{
    if (settings == NULL || !settings->shelly_co2.enabled) {
        return false;
    }

    const char *ip = settings->shelly_co2.ip;
    if (ip[0] == '\0') {
        return false;
    }

    if (strncmp(ip, settings->shelly_filter.ip, FISHDUINO_IP_LEN) == 0) {
        ESP_LOGE(TAG, "Refusing Switch.Set: CO2 IP matches filter plug");
        return false;
    }

    return shelly_set_switch_output(ip, settings->shelly_co2.switch_id, on);
}

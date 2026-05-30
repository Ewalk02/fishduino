#include "fluval_ble_client.h"
#include "fluval_protocol.h"

#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "host/ble_gap.h"
#include "host/ble_gatt.h"
#include "host/ble_hs.h"
#include "host/ble_uuid.h"
#include "host/util/util.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"

static const char *TAG = "fluval_ble_cli";

static const ble_uuid128_t FLUVAL_SVC_UUID =
    BLE_UUID128_INIT(0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00, 0xf0, 0xff, 0x00, 0x00);
static const ble_uuid128_t FLUVAL_NOTIFY_UUID =
    BLE_UUID128_INIT(0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00, 0xf1, 0xff, 0x00, 0x00);
static const ble_uuid128_t FLUVAL_WRITE_UUID =
    BLE_UUID128_INIT(0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00, 0xf2, 0xff, 0x00, 0x00);
static const ble_uuid128_t FLUVAL_EXTRA_UUID =
    BLE_UUID128_INIT(0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00, 0xf3, 0xff, 0x00, 0x00);

#define FLUVAL_EVT_ACK     BIT0
#define FLUVAL_EVT_STATUS  BIT1

typedef enum {
    BLE_JOB_NONE = 0,
    BLE_JOB_WRITE,
    BLE_JOB_STATUS,
} ble_job_type_t;

typedef struct {
    ble_job_type_t type;
    uint8_t data[FLUVAL_PROTOCOL_SET_CHANNELS_LEN];
    size_t len;
    fluval_protocol_ack_t expect_ack;
} ble_job_t;

typedef struct {
    bool ready;
    bool connected;
    bool subscribed;

    fishduino_fluval_ble_client_state_t state;
    char target_name[32];
    uint32_t poll_interval_ms;
    uint32_t stale_timeout_ms;
    SemaphoreHandle_t state_mutex;
    EventGroupHandle_t wait_events;

    fluval_protocol_ack_t pending_ack;
    bool waiting_ack;
    bool waiting_status;

    QueueHandle_t job_queue;

    uint16_t conn_handle;
    uint16_t notify_handle;
    uint16_t write_handle;
    uint16_t notify_cccd_handle;

    char adv_name[32];
    ble_addr_t adv_addr;
    int adv_rssi;
    bool have_adv_target;

    uint32_t backoff_ms;
    uint64_t next_action_us;
    uint32_t last_poll_ms;

    bool auto_connect;
} fluval_ble_ctx_t;

static fluval_ble_ctx_t s_ble;

static int discover_svc_cb(uint16_t conn_handle, const struct ble_gatt_error *error, const struct ble_gatt_svc *svc,
                           void *arg);

static uint32_t now_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000ULL);
}

static void state_lock(void)
{
    if (s_ble.state_mutex != NULL) {
        xSemaphoreTake(s_ble.state_mutex, portMAX_DELAY);
    }
}

static void state_unlock(void)
{
    if (s_ble.state_mutex != NULL) {
        xSemaphoreGive(s_ble.state_mutex);
    }
}

static bool name_matches(const char *name)
{
    return name != NULL && s_ble.target_name[0] != '\0' && strcmp(name, s_ble.target_name) == 0;
}

static int write_cmd(const uint8_t *data, size_t len)
{
    if (!s_ble.connected || s_ble.write_handle == 0 || data == NULL || len == 0) {
        return -1;
    }

    struct os_mbuf *om = ble_hs_mbuf_from_flat(data, len);
    if (om == NULL) {
        return -1;
    }
    return ble_gattc_write_no_rsp(s_ble.conn_handle, s_ble.write_handle, om);
}

static void update_state_from_parsed(const fluval_protocol_status_t *parsed)
{
    if (parsed == NULL || !parsed->valid) {
        return;
    }

    state_lock();
    s_ble.state.protocol = *parsed;
    s_ble.state.rssi = s_ble.adv_rssi;
    s_ble.state.last_update_ms = now_ms();
    state_unlock();
}

static void handle_notify(struct os_mbuf *om)
{
    if (om == NULL) {
        return;
    }

    uint8_t buf[FLUVAL_BLE_NOTIFY_MAX_LEN] = {0};
    int len = ble_hs_mbuf_to_flat(om, buf, sizeof(buf), NULL);
    if (len <= 0) {
        return;
    }

    if (esp_log_level_get(TAG) >= ESP_LOG_DEBUG) {
        char hex[FLUVAL_BLE_NOTIFY_MAX_LEN * 3 + 1] = {0};
        size_t off = 0;
        for (int i = 0; i < len && off + 3 < sizeof(hex); i++) {
            off += (size_t)snprintf(&hex[off], sizeof(hex) - off, "%02x", buf[i]);
        }
        ESP_LOGD(TAG, "notify len=%d: %s", len, hex);
    }

    fluval_protocol_status_t parsed = {0};
    fluval_protocol_ack_t ack = fluval_protocol_parse_ack(buf, (size_t)len, &parsed);

    switch (ack) {
    case FLUVAL_PROTOCOL_ACK_MODE_MANUAL:
    case FLUVAL_PROTOCOL_ACK_MODE_AUTO:
    case FLUVAL_PROTOCOL_ACK_SET_CHANNELS:
        if (s_ble.waiting_ack &&
            (s_ble.pending_ack == FLUVAL_PROTOCOL_ACK_NONE || s_ble.pending_ack == ack)) {
            xEventGroupSetBits(s_ble.wait_events, FLUVAL_EVT_ACK);
        }
        break;
    case FLUVAL_PROTOCOL_ACK_STATUS:
        update_state_from_parsed(&parsed);
        ESP_LOGI(TAG, "status: mode=%s avg=%u P=%u B=%u CW=%u W=%u WW=%u", fluval_protocol_mode_token(parsed.mode),
                 (unsigned)parsed.avg_output, (unsigned)parsed.pink, (unsigned)parsed.blue,
                 (unsigned)parsed.cold_white, (unsigned)parsed.white, (unsigned)parsed.warm_white);
        if (s_ble.waiting_status) {
            xEventGroupSetBits(s_ble.wait_events, FLUVAL_EVT_STATUS);
        }
        break;
    default:
        break;
    }
}

static int gap_event(struct ble_gap_event *event, void *arg)
{
    (void)arg;

    switch (event->type) {
    case BLE_GAP_EVENT_DISC: {
        struct ble_hs_adv_fields fields;
        if (ble_hs_adv_parse_fields(&fields, event->disc.data, event->disc.length_data) != 0) {
            return 0;
        }
        if (fields.name == NULL || fields.name_len == 0) {
            return 0;
        }

        char tmp[sizeof(s_ble.adv_name)] = {0};
        size_t copy = fields.name_len < sizeof(tmp) - 1 ? (size_t)fields.name_len : sizeof(tmp) - 1;
        memcpy(tmp, fields.name, copy);
        tmp[copy] = '\0';

        if (!name_matches(tmp)) {
            return 0;
        }

        ESP_LOGI(TAG, "found target name=%s rssi=%d", tmp, event->disc.rssi);
        strncpy(s_ble.adv_name, tmp, sizeof(s_ble.adv_name) - 1);
        s_ble.adv_addr = event->disc.addr;
        s_ble.adv_rssi = event->disc.rssi;
        s_ble.have_adv_target = true;
        ble_gap_disc_cancel();
        return 0;
    }

    case BLE_GAP_EVENT_CONNECT:
        if (event->connect.status != 0) {
            ESP_LOGW(TAG, "connect failed status=%d", event->connect.status);
            s_ble.connected = false;
            s_ble.subscribed = false;
            s_ble.conn_handle = BLE_HS_CONN_HANDLE_NONE;
            s_ble.have_adv_target = false;
            if (s_ble.backoff_ms < 30000) {
                s_ble.backoff_ms *= 2;
            }
            return 0;
        }

        s_ble.conn_handle = event->connect.conn_handle;
        s_ble.connected = true;
        s_ble.subscribed = false;
        s_ble.notify_handle = 0;
        s_ble.write_handle = 0;
        s_ble.notify_cccd_handle = 0;
        s_ble.backoff_ms = 500;
        ESP_LOGI(TAG, "connected");
        ble_gattc_disc_all_svcs(s_ble.conn_handle, discover_svc_cb, NULL);
        return 0;

    case BLE_GAP_EVENT_DISCONNECT:
        ESP_LOGW(TAG, "disconnected reason=%d", event->disconnect.reason);
        s_ble.connected = false;
        s_ble.subscribed = false;
        s_ble.conn_handle = BLE_HS_CONN_HANDLE_NONE;
        s_ble.notify_handle = 0;
        s_ble.write_handle = 0;
        s_ble.notify_cccd_handle = 0;
        s_ble.have_adv_target = false;
        if (s_ble.backoff_ms < 30000) {
            s_ble.backoff_ms *= 2;
        }
        s_ble.next_action_us = esp_timer_get_time() + (uint64_t)s_ble.backoff_ms * 1000ULL;
        xEventGroupSetBits(s_ble.wait_events, FLUVAL_EVT_ACK | FLUVAL_EVT_STATUS);
        return 0;

    case BLE_GAP_EVENT_NOTIFY_RX:
        handle_notify(event->notify_rx.om);
        return 0;

    default:
        return 0;
    }
}

static int discover_chr_cb(uint16_t conn_handle, const struct ble_gatt_error *error, const struct ble_gatt_chr *chr,
                           void *arg)
{
    (void)conn_handle;
    (void)arg;

    if (error != NULL && error->status != 0) {
        return 0;
    }
    if (chr == NULL) {
        return 0;
    }

    if (ble_uuid_cmp(&chr->uuid.u, &FLUVAL_NOTIFY_UUID.u) == 0) {
        s_ble.notify_handle = chr->val_handle;
        ESP_LOGI(TAG, "FFF1 notify handle=0x%04x", s_ble.notify_handle);
    } else if (ble_uuid_cmp(&chr->uuid.u, &FLUVAL_WRITE_UUID.u) == 0) {
        s_ble.write_handle = chr->val_handle;
        ESP_LOGI(TAG, "FFF2 write handle=0x%04x", s_ble.write_handle);
    } else if (ble_uuid_cmp(&chr->uuid.u, &FLUVAL_EXTRA_UUID.u) == 0) {
        ESP_LOGI(TAG, "FFF3 handle=0x%04x", chr->val_handle);
    }

    return 0;
}

static int discover_dsc_cb(uint16_t conn_handle, const struct ble_gatt_error *error, uint16_t chr_val_handle,
                           const struct ble_gatt_dsc *dsc, void *arg)
{
    (void)conn_handle;
    (void)chr_val_handle;
    (void)arg;

    if (error != NULL) {
        if (error->status == BLE_HS_EDONE) {
            return 0;
        }
        if (error->status != 0) {
            return 0;
        }
    }
    if (dsc == NULL) {
        return 0;
    }

    ble_uuid16_t cccd_uuid = BLE_UUID16_INIT(0x2902);
    if (ble_uuid_cmp(&dsc->uuid.u, &cccd_uuid.u) == 0) {
        s_ble.notify_cccd_handle = dsc->handle;
        ESP_LOGI(TAG, "FFF1 CCCD handle=0x%04x", s_ble.notify_cccd_handle);
    }
    return 0;
}

static int subscribe_complete_cb(uint16_t conn_handle, const struct ble_gatt_error *error, struct ble_gatt_attr *attr,
                                 void *arg)
{
    (void)conn_handle;
    (void)attr;
    (void)arg;

    if (error != NULL && error->status != 0) {
        ESP_LOGW(TAG, "CCCD write failed status=%d", error->status);
        return 0;
    }

    s_ble.subscribed = true;
    ESP_LOGI(TAG, "subscribed to FFF1 notifications");

    uint8_t query[FLUVAL_PROTOCOL_STATUS_QUERY_LEN];
    size_t qlen = fluval_protocol_build_status_query(query, sizeof(query));
    if (qlen > 0) {
        write_cmd(query, qlen);
    }
    s_ble.last_poll_ms = now_ms();
    return 0;
}

static int discover_svc_cb(uint16_t conn_handle, const struct ble_gatt_error *error, const struct ble_gatt_svc *svc,
                           void *arg)
{
    (void)conn_handle;
    (void)arg;

    if (error != NULL && error->status != 0) {
        return 0;
    }
    if (svc == NULL) {
        return 0;
    }

    if (ble_uuid_cmp(&svc->uuid.u, &FLUVAL_SVC_UUID.u) == 0) {
        ESP_LOGI(TAG, "found FFF0 service");
        ble_gattc_disc_all_chrs(s_ble.conn_handle, svc->start_handle, svc->end_handle, discover_chr_cb, NULL);
    }
    return 0;
}

static void maybe_finish_discovery(void)
{
    if (!s_ble.connected) {
        return;
    }

    if (s_ble.notify_handle != 0 && s_ble.write_handle != 0 && s_ble.notify_cccd_handle == 0) {
        ble_gattc_disc_all_dscs(s_ble.conn_handle, s_ble.notify_handle, s_ble.notify_handle + 8, discover_dsc_cb,
                                NULL);
    }

    if (s_ble.notify_cccd_handle != 0 && !s_ble.subscribed) {
        uint8_t cccd_val[2] = {0x01, 0x00};
        int rc = ble_gattc_write_flat(s_ble.conn_handle, s_ble.notify_cccd_handle, cccd_val, sizeof(cccd_val),
                                      subscribe_complete_cb, NULL);
        if (rc != 0) {
            ESP_LOGW(TAG, "CCCD write rc=%d", rc);
        }
    }
}

static int scan_start(void)
{
    struct ble_gap_disc_params params = {0};
    params.passive = 0;
    params.itvl = 0x0010;
    params.window = 0x0010;
    params.filter_duplicates = 1;

    s_ble.have_adv_target = false;
    ESP_LOGI(TAG, "scanning for %s", s_ble.target_name);
    return ble_gap_disc(BLE_OWN_ADDR_PUBLIC, BLE_HS_FOREVER, &params, gap_event, NULL);
}

static int connect_to_found(void)
{
    if (!s_ble.have_adv_target) {
        return 0;
    }

    ESP_LOGI(TAG, "connecting to %s", s_ble.adv_name);
    struct ble_gap_conn_params conn_params = {0};
    conn_params.scan_itvl = 0x0010;
    conn_params.scan_window = 0x0010;
    conn_params.itvl_min = 0x0010;
    conn_params.itvl_max = 0x0020;
    conn_params.latency = 0;
    conn_params.supervision_timeout = 0x0100;
    conn_params.min_ce_len = 0x0010;
    conn_params.max_ce_len = 0x0030;

    return ble_gap_connect(BLE_OWN_ADDR_PUBLIC, &s_ble.adv_addr, 30000, &conn_params, gap_event, NULL);
}

static void update_stale(void)
{
    state_lock();
    if (s_ble.state.last_update_ms == 0) {
        s_ble.state.protocol.valid = false;
    } else {
        uint32_t age = now_ms() - s_ble.state.last_update_ms;
        if (age > s_ble.stale_timeout_ms) {
            s_ble.state.protocol.valid = false;
        }
    }
    state_unlock();
}

static void maybe_poll_status(void)
{
    if (!s_ble.connected || !s_ble.subscribed) {
        return;
    }

    uint32_t t = now_ms();
    if (s_ble.last_poll_ms != 0 && (t - s_ble.last_poll_ms) < s_ble.poll_interval_ms) {
        return;
    }

    uint8_t query[FLUVAL_PROTOCOL_STATUS_QUERY_LEN];
    size_t qlen = fluval_protocol_build_status_query(query, sizeof(query));
    if (qlen > 0 && write_cmd(query, qlen) == 0) {
        s_ble.last_poll_ms = t;
    }
}

static esp_err_t wait_bits(EventBits_t bits, uint32_t timeout_ms)
{
    EventBits_t got =
        xEventGroupWaitBits(s_ble.wait_events, bits, pdTRUE, pdFALSE, pdMS_TO_TICKS(timeout_ms));
    if ((got & bits) == bits) {
        return ESP_OK;
    }
    return ESP_ERR_TIMEOUT;
}

static esp_err_t send_and_wait_ack_status(const uint8_t *data, size_t len, fluval_protocol_ack_t expect_ack)
{
    if (!fishduino_fluval_ble_client_is_connected()) {
        return ESP_ERR_INVALID_STATE;
    }

    xEventGroupClearBits(s_ble.wait_events, FLUVAL_EVT_ACK | FLUVAL_EVT_STATUS);
    s_ble.pending_ack = expect_ack;
    s_ble.waiting_ack = true;
    s_ble.waiting_status = false;

    if (write_cmd(data, len) != 0) {
        s_ble.waiting_ack = false;
        return ESP_FAIL;
    }

    esp_err_t err = wait_bits(FLUVAL_EVT_ACK, FLUVAL_BLE_CMD_WAIT_MS);
    s_ble.waiting_ack = false;
    if (err != ESP_OK) {
        return err;
    }

    uint8_t query[FLUVAL_PROTOCOL_STATUS_QUERY_LEN];
    size_t qlen = fluval_protocol_build_status_query(query, sizeof(query));
    if (qlen == 0) {
        return ESP_FAIL;
    }

    xEventGroupClearBits(s_ble.wait_events, FLUVAL_EVT_STATUS);
    s_ble.waiting_status = true;
    if (write_cmd(query, qlen) != 0) {
        s_ble.waiting_status = false;
        return ESP_FAIL;
    }

    err = wait_bits(FLUVAL_EVT_STATUS, FLUVAL_BLE_CMD_WAIT_MS);
    s_ble.waiting_status = false;
    return err;
}

static void pump_connect(void)
{
    uint64_t now_us = esp_timer_get_time();
    if (now_us < s_ble.next_action_us) {
        return;
    }

    if (s_ble.connected) {
        maybe_finish_discovery();
        if (s_ble.connected && s_ble.notify_handle == 0 && s_ble.write_handle == 0) {
            ble_gattc_disc_all_svcs(s_ble.conn_handle, discover_svc_cb, NULL);
        }
        return;
    }

    if (!s_ble.have_adv_target) {
        int rc = scan_start();
        if (rc != 0) {
            ESP_LOGW(TAG, "scan_start rc=%d", rc);
        }
        s_ble.next_action_us = now_us + (uint64_t)s_ble.backoff_ms * 1000ULL;
        return;
    }

    int rc = connect_to_found();
    if (rc != 0) {
        ESP_LOGW(TAG, "connect rc=%d", rc);
        s_ble.have_adv_target = false;
        if (s_ble.backoff_ms < 30000) {
            s_ble.backoff_ms *= 2;
        }
    }
    s_ble.next_action_us = now_us + (uint64_t)s_ble.backoff_ms * 1000ULL;
}

static void ble_worker_task(void *arg)
{
    (void)arg;
    ble_job_t job;

    while (true) {
        pump_connect();
        update_stale();
        maybe_poll_status();

        if (xQueueReceive(s_ble.job_queue, &job, pdMS_TO_TICKS(50)) == pdTRUE) {
            if (job.type == BLE_JOB_STATUS) {
                uint8_t query[FLUVAL_PROTOCOL_STATUS_QUERY_LEN];
                size_t qlen = fluval_protocol_build_status_query(query, sizeof(query));
                if (qlen > 0) {
                    write_cmd(query, qlen);
                }
            } else if (job.type == BLE_JOB_WRITE && job.len > 0) {
                write_cmd(job.data, job.len);
            }
        }
    }
}

static void ble_host_task(void *param)
{
    (void)param;
    nimble_port_run();
    nimble_port_freertos_deinit();
    vTaskDelete(NULL);
}

static void on_sync(void)
{
    ble_hs_id_infer_auto(0, NULL);
    s_ble.auto_connect = true;
    s_ble.next_action_us = esp_timer_get_time();
}

esp_err_t fishduino_fluval_ble_client_set_config(const fishduino_fluval_ble_client_config_t *cfg)
{
    if (cfg == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    strncpy(s_ble.target_name, cfg->target_name, sizeof(s_ble.target_name) - 1);
    s_ble.target_name[sizeof(s_ble.target_name) - 1] = '\0';
    s_ble.poll_interval_ms = cfg->poll_interval_ms > 0 ? cfg->poll_interval_ms : 10000;
    s_ble.stale_timeout_ms = cfg->stale_timeout_ms > 0 ? cfg->stale_timeout_ms : 30000;
    return ESP_OK;
}

esp_err_t fishduino_fluval_ble_client_init(void)
{
    if (s_ble.ready) {
        return ESP_OK;
    }

    memset(&s_ble, 0, sizeof(s_ble));
    s_ble.conn_handle = BLE_HS_CONN_HANDLE_NONE;
    s_ble.backoff_ms = 500;
    s_ble.adv_rssi = 0;
    snprintf(s_ble.target_name, sizeof(s_ble.target_name), "%s", "Plant4.0_450467");
    s_ble.poll_interval_ms = 10000;
    s_ble.stale_timeout_ms = 30000;

    s_ble.state_mutex = xSemaphoreCreateMutex();
    s_ble.wait_events = xEventGroupCreate();
    s_ble.job_queue = xQueueCreate(4, sizeof(ble_job_t));
    if (s_ble.state_mutex == NULL || s_ble.wait_events == NULL || s_ble.job_queue == NULL) {
        return ESP_ERR_NO_MEM;
    }

    nimble_port_init();
    ble_hs_cfg.sync_cb = on_sync;
    nimble_port_freertos_init(ble_host_task);

    s_ble.ready = true;
    return ESP_OK;
}

esp_err_t fishduino_fluval_ble_client_start(void)
{
    if (!s_ble.ready) {
        return ESP_ERR_INVALID_STATE;
    }

    static bool worker_started;
    if (worker_started) {
        return ESP_OK;
    }

    BaseType_t ok = xTaskCreate(ble_worker_task, "fluval_ble", 4096, NULL, 5, NULL);
    if (ok != pdPASS) {
        return ESP_ERR_NO_MEM;
    }
    worker_started = true;
    return ESP_OK;
}

bool fishduino_fluval_ble_client_is_connected(void)
{
    return s_ble.ready && s_ble.connected && s_ble.subscribed;
}

esp_err_t fishduino_fluval_ble_client_get_state(fishduino_fluval_ble_client_state_t *out)
{
    if (out == NULL || s_ble.state_mutex == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (xSemaphoreTake(s_ble.state_mutex, pdMS_TO_TICKS(200)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    *out = s_ble.state;
    xSemaphoreGive(s_ble.state_mutex);
    return ESP_OK;
}

esp_err_t fishduino_fluval_ble_client_request_status(void)
{
    if (!fishduino_fluval_ble_client_is_connected()) {
        return ESP_ERR_INVALID_STATE;
    }

    ble_job_t job = {.type = BLE_JOB_STATUS};
    if (xQueueSend(s_ble.job_queue, &job, 0) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    return ESP_OK;
}

esp_err_t fishduino_fluval_ble_client_wait_status(fishduino_fluval_ble_client_state_t *out, uint32_t timeout_ms)
{
    if (!fishduino_fluval_ble_client_is_connected()) {
        return ESP_ERR_INVALID_STATE;
    }

    xEventGroupClearBits(s_ble.wait_events, FLUVAL_EVT_STATUS);
    s_ble.waiting_status = true;

    esp_err_t err = fishduino_fluval_ble_client_request_status();
    if (err != ESP_OK) {
        s_ble.waiting_status = false;
        return err;
    }

    err = wait_bits(FLUVAL_EVT_STATUS, timeout_ms);
    s_ble.waiting_status = false;
    if (err != ESP_OK) {
        return err;
    }

    if (out != NULL) {
        return fishduino_fluval_ble_client_get_state(out);
    }
    return ESP_OK;
}

esp_err_t fishduino_fluval_ble_client_set_mode_manual(void)
{
    uint8_t cmd[FLUVAL_PROTOCOL_SET_MANUAL_LEN];
    size_t len = fluval_protocol_build_set_manual(cmd, sizeof(cmd));
    if (len == 0) {
        return ESP_FAIL;
    }
    return send_and_wait_ack_status(cmd, len, FLUVAL_PROTOCOL_ACK_MODE_MANUAL);
}

esp_err_t fishduino_fluval_ble_client_set_mode_auto(void)
{
    uint8_t cmd[FLUVAL_PROTOCOL_SET_AUTO_LEN];
    size_t len = fluval_protocol_build_set_auto(cmd, sizeof(cmd));
    if (len == 0) {
        return ESP_FAIL;
    }
    return send_and_wait_ack_status(cmd, len, FLUVAL_PROTOCOL_ACK_MODE_AUTO);
}

esp_err_t fishduino_fluval_ble_client_set_channels(uint8_t pink, uint8_t blue, uint8_t cold_white, uint8_t white, uint8_t warm_white)
{
    uint8_t cmd[FLUVAL_PROTOCOL_SET_CHANNELS_LEN];
    size_t len = fluval_protocol_build_set_channels(pink, blue, cold_white, white, warm_white, cmd, sizeof(cmd));
    if (len == 0) {
        return ESP_FAIL;
    }
    return send_and_wait_ack_status(cmd, len, FLUVAL_PROTOCOL_ACK_SET_CHANNELS);
}

esp_err_t fishduino_fluval_ble_client_set_all(uint8_t percent)
{
    uint8_t p = fluval_protocol_clamp_percent(percent);
    return fishduino_fluval_ble_client_set_channels(p, p, p, p, p);
}

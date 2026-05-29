#include "chihiros_heater_client.h"

#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_timer.h"

#include "host/ble_hs.h"
#include "host/ble_uuid.h"
#include "host/util/util.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"

static const char *TAG = "chihiros_ble";

// Nordic UART Service UUIDs
static const ble_uuid128_t NUS_SVC_UUID =
    BLE_UUID128_INIT(0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa0, 0xe0, 0x93, 0xf3, 0xa3, 0xb5, 0x01, 0x00, 0x40, 0x6e);
static const ble_uuid128_t NUS_RX_UUID =
    BLE_UUID128_INIT(0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa0, 0xe0, 0x93, 0xf3, 0xa3, 0xb5, 0x02, 0x00, 0x40, 0x6e);
static const ble_uuid128_t NUS_TX_UUID =
    BLE_UUID128_INIT(0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa0, 0xe0, 0x93, 0xf3, 0xa3, 0xb5, 0x03, 0x00, 0x40, 0x6e);

static const uint8_t INIT_1[] = {0x5a, 0x01, 0x06, 0x00, 0x02, 0x04, 0x01, 0x00};
static const uint8_t INIT_2[] = {0x5a, 0x01, 0x0b, 0x00, 0x03, 0x09, 0x1a, 0x05, 0x02, 0x13, 0x25, 0x0c, 0x27};
static const uint8_t INIT_3[] = {0x5a, 0x01, 0x06, 0x00, 0x04, 0x04, 0x01, 0x06};

struct chihiros_heater_client {
    chihiros_heater_client_config_t cfg;
    chihiros_heater_client_state_cb_t cb;
    void *cb_ctx;

    chihiros_heater_client_state_t state;

    // NimBLE handles
    uint16_t conn_handle;
    uint16_t nus_rx_handle;
    uint16_t nus_tx_handle;
    uint16_t nus_tx_cccd_handle;

    bool connect_requested;
    bool disconnect_requested;

    uint32_t backoff_ms;
    uint64_t next_action_us;

    char adv_name[32];
    ble_addr_t adv_addr;
    bool have_adv_target;
};

static uint32_t now_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000ULL);
}

static void emit_state(chihiros_heater_client_t *c)
{
    if (c->cb) {
        c->cb(&c->state, c->cb_ctx);
    }
}

static bool name_has_prefix(const char *name, const char *prefix)
{
    if (name == NULL || prefix == NULL) return false;
    size_t n = strlen(prefix);
    return strncmp(name, prefix, n) == 0;
}

static void handle_notify(chihiros_heater_client_t *c, struct os_mbuf *om);
static int subscribe_tx_notify(chihiros_heater_client_t *c);
static int send_init_sequence(chihiros_heater_client_t *c);
static int discover_nus(chihiros_heater_client_t *c);

static int gap_event(struct ble_gap_event *event, void *arg)
{
    chihiros_heater_client_t *c = (chihiros_heater_client_t *)arg;

    switch (event->type) {
    case BLE_GAP_EVENT_DISC: {
        // Parse advertisement for complete/local name.
        struct ble_hs_adv_fields fields;
        int rc = ble_hs_adv_parse_fields(&fields, event->disc.data, event->disc.length_data);
        if (rc != 0) {
            return 0;
        }

        const uint8_t *name = fields.name;
        uint8_t name_len = fields.name_len;
        if (name == NULL || name_len == 0) {
            return 0;
        }

        char tmp[sizeof(c->adv_name)] = {0};
        size_t copy = name_len < sizeof(tmp) - 1 ? (size_t)name_len : sizeof(tmp) - 1;
        memcpy(tmp, name, copy);
        tmp[copy] = '\0';

        if (!name_has_prefix(tmp, c->cfg.name_prefix)) {
            return 0;
        }

        ESP_LOGI(TAG, "found heater adv name=%s", tmp);
        strncpy(c->adv_name, tmp, sizeof(c->adv_name));
        c->adv_addr = event->disc.addr;
        c->have_adv_target = true;

        // Stop scanning; we'll connect shortly.
        ble_gap_disc_cancel();
        return 0;
    }

    case BLE_GAP_EVENT_DISC_COMPLETE:
        return 0;

    case BLE_GAP_EVENT_CONNECT:
        if (event->connect.status != 0) {
            ESP_LOGW(TAG, "connect failed status=%d", event->connect.status);
            c->state.connected = false;
            c->state.subscribed = false;
            c->conn_handle = BLE_HS_CONN_HANDLE_NONE;
            c->have_adv_target = false;
            if (c->backoff_ms < 30000) c->backoff_ms *= 2;
            emit_state(c);
            return 0;
        }

        c->conn_handle = event->connect.conn_handle;
        c->state.connected = true;
        c->state.subscribed = false;
        emit_state(c);

        ESP_LOGI(TAG, "connected; discovering NUS");
        discover_nus(c);
        return 0;

    case BLE_GAP_EVENT_DISCONNECT:
        ESP_LOGW(TAG, "disconnected reason=%d", event->disconnect.reason);
        c->state.connected = false;
        c->state.subscribed = false;
        c->conn_handle = BLE_HS_CONN_HANDLE_NONE;
        c->nus_rx_handle = 0;
        c->nus_tx_handle = 0;
        c->nus_tx_cccd_handle = 0;
        c->have_adv_target = false;
        // Back off a bit before re-scanning.
        if (c->backoff_ms < 30000) c->backoff_ms *= 2;
        c->next_action_us = esp_timer_get_time() + (uint64_t)c->backoff_ms * 1000ULL;
        emit_state(c);
        return 0;

    case BLE_GAP_EVENT_NOTIFY_RX:
        // Incoming notifications are delivered here on NimBLE (ESP-IDF).
        handle_notify(c, event->notify_rx.om);
        return 0;

    default:
        return 0;
    }
}

static void handle_notify(chihiros_heater_client_t *c, struct os_mbuf *om)
{
    if (om == NULL) return;

    uint8_t buf[64] = {0};
    int len = ble_hs_mbuf_to_flat(om, buf, sizeof(buf), NULL);
    if (len <= 0) return;

    char hex[3 * 64] = {0};
    size_t off = 0;
    for (int i = 0; i < len && off + 3 < sizeof(hex); i++) {
        off += (size_t)snprintf(&hex[off], sizeof(hex) - off, "%02x ", buf[i]);
    }
    ESP_LOGI(TAG, "notify: %s", hex);

    chihiros_status_t st = {0};
    if (chihiros_decode_status_packet(buf, (size_t)len, &st)) {
        c->state.last_status = st;
        c->state.last_status_ms = now_ms();
        c->state.stale = false;
        ESP_LOGI(TAG, "decoded: %.2fC %.2fF watts=%u heating=%s",
                 (double)st.current_temp_c, (double)st.current_temp_f,
                 (unsigned)st.watts, st.heating ? "yes" : "no");
        emit_state(c);
    }
}

static int scan_start(chihiros_heater_client_t *c)
{
    struct ble_gap_disc_params params = {0};
    params.passive = 0;
    params.itvl = 0x0010;
    params.window = 0x0010;
    params.filter_duplicates = 1;

    c->have_adv_target = false;
    ESP_LOGI(TAG, "scanning for %s*", c->cfg.name_prefix);
    return ble_gap_disc(BLE_OWN_ADDR_PUBLIC, BLE_HS_FOREVER, &params, gap_event, c);
}

static int connect_to_found(chihiros_heater_client_t *c)
{
    if (!c->have_adv_target) {
        return 0;
    }

    ESP_LOGI(TAG, "connecting to %s", c->adv_name);
    struct ble_gap_conn_params conn_params = {0};
    conn_params.scan_itvl = 0x0010;
    conn_params.scan_window = 0x0010;
    conn_params.itvl_min = 0x0010;
    conn_params.itvl_max = 0x0020;
    conn_params.latency = 0;
    conn_params.supervision_timeout = 0x0100;
    conn_params.min_ce_len = 0x0010;
    conn_params.max_ce_len = 0x0030;

    return ble_gap_connect(BLE_OWN_ADDR_PUBLIC, &c->adv_addr, 30000, &conn_params, gap_event, c);
}

static int discover_chr_cb(uint16_t conn_handle, const struct ble_gatt_error *error,
                           const struct ble_gatt_chr *chr, void *arg)
{
    (void)conn_handle;
    chihiros_heater_client_t *c = (chihiros_heater_client_t *)arg;

    if (error && error->status != 0) {
        ESP_LOGW(TAG, "discover chr error=%d", error->status);
        return 0;
    }
    if (chr == NULL) {
        return 0;
    }

    if (ble_uuid_cmp(&chr->uuid.u, &NUS_RX_UUID.u) == 0) {
        c->nus_rx_handle = chr->val_handle;
    } else if (ble_uuid_cmp(&chr->uuid.u, &NUS_TX_UUID.u) == 0) {
        c->nus_tx_handle = chr->val_handle;
    }

    return 0;
}

static int discover_svc_cb(uint16_t conn_handle, const struct ble_gatt_error *error,
                           const struct ble_gatt_svc *svc, void *arg)
{
    (void)conn_handle;
    chihiros_heater_client_t *c = (chihiros_heater_client_t *)arg;

    if (error && error->status != 0) {
        ESP_LOGW(TAG, "discover svc error=%d", error->status);
        return 0;
    }
    if (svc == NULL) {
        return 0;
    }

    if (ble_uuid_cmp(&svc->uuid.u, &NUS_SVC_UUID.u) == 0) {
        ESP_LOGI(TAG, "found NUS svc; discovering chrs");
        ble_gattc_disc_all_chrs(c->conn_handle, svc->start_handle, svc->end_handle, discover_chr_cb, c);
    }
    return 0;
}

static int discover_dsc_cb(uint16_t conn_handle, const struct ble_gatt_error *error,
                           uint16_t chr_val_handle, const struct ble_gatt_dsc *dsc, void *arg)
{
    (void)conn_handle;
    (void)chr_val_handle;
    chihiros_heater_client_t *c = (chihiros_heater_client_t *)arg;

    if (error) {
        if (error->status == BLE_HS_EDONE) {
            // Discovery complete. Nothing to do here.
            return 0;
        }
        if (error->status != 0) {
            ESP_LOGW(TAG, "discover dsc error=%d", error->status);
            return 0;
        }
    }
    if (dsc == NULL) return 0;

    // CCCD UUID = 0x2902
    ble_uuid16_t cccd_uuid = BLE_UUID16_INIT(0x2902);
    if (ble_uuid_cmp(&dsc->uuid.u, &cccd_uuid.u) == 0) {
        c->nus_tx_cccd_handle = dsc->handle;
    }
    return 0;
}

static int subscribe_complete_cb(uint16_t conn_handle, const struct ble_gatt_error *error,
                                 struct ble_gatt_attr *attr, void *arg)
{
    (void)conn_handle;
    (void)attr;
    chihiros_heater_client_t *c = (chihiros_heater_client_t *)arg;

    if (error && error->status != 0) {
        ESP_LOGW(TAG, "subscribe write cccd failed=%d", error->status);
        return 0;
    }

    c->state.subscribed = true;
    emit_state(c);
    ESP_LOGI(TAG, "subscribed; sending init sequence");
    send_init_sequence(c);
    return 0;
}

static int subscribe_tx_notify(chihiros_heater_client_t *c)
{
    if (c->nus_tx_handle == 0) {
        ESP_LOGW(TAG, "no nus_tx_handle yet");
        return -1;
    }

    // Discover CCCD then write 0x0001 to enable notifications.
    c->nus_tx_cccd_handle = 0;
    int rc = ble_gattc_disc_all_dscs(c->conn_handle, c->nus_tx_handle, c->nus_tx_handle + 8, discover_dsc_cb, c);
    if (rc != 0) {
        ESP_LOGW(TAG, "disc dscs rc=%d", rc);
        return rc;
    }

    // Best-effort: if CCCD isn't discovered immediately, we still proceed with a direct guess is unsafe.
    // We'll poll later in maybe_finish_discovery().
    return 0;
}

static int write_no_rsp(const uint8_t *data, size_t len, chihiros_heater_client_t *c)
{
    if (!c->state.connected || c->nus_rx_handle == 0) return -1;
    struct os_mbuf *om = ble_hs_mbuf_from_flat(data, len);
    if (!om) return -1;
    return ble_gattc_write_no_rsp(c->conn_handle, c->nus_rx_handle, om);
}

static int send_init_sequence(chihiros_heater_client_t *c)
{
    // Must subscribe to notify before init; caller enforces.
    int rc = 0;
    rc = write_no_rsp(INIT_1, sizeof(INIT_1), c);
    if (rc) ESP_LOGW(TAG, "init1 rc=%d", rc);
    rc = write_no_rsp(INIT_2, sizeof(INIT_2), c);
    if (rc) ESP_LOGW(TAG, "init2 rc=%d", rc);
    rc = write_no_rsp(INIT_3, sizeof(INIT_3), c);
    if (rc) ESP_LOGW(TAG, "init3 rc=%d", rc);
    return 0;
}

static int discover_nus(chihiros_heater_client_t *c)
{
    c->nus_rx_handle = 0;
    c->nus_tx_handle = 0;
    c->nus_tx_cccd_handle = 0;
    return ble_gattc_disc_all_svcs(c->conn_handle, discover_svc_cb, c);
}

static void maybe_finish_discovery(chihiros_heater_client_t *c)
{
    if (c->nus_rx_handle && c->nus_tx_handle && c->nus_tx_cccd_handle == 0) {
        // Start descriptor discovery (to find CCCD) once.
        ESP_LOGI(TAG, "NUS discovered (rx=0x%04x tx=0x%04x), discovering CCCD", c->nus_rx_handle, c->nus_tx_handle);
        subscribe_tx_notify(c);
    }

    if (c->nus_tx_cccd_handle != 0 && !c->state.subscribed) {
        // Write CCCD = 0x0001 (notifications enabled).
        uint8_t cccd_val[2] = {0x01, 0x00};
        ESP_LOGI(TAG, "writing CCCD handle=0x%04x", c->nus_tx_cccd_handle);
        int rc = ble_gattc_write_flat(c->conn_handle, c->nus_tx_cccd_handle,
                                      cccd_val, sizeof(cccd_val),
                                      subscribe_complete_cb, c);
        if (rc != 0) {
            ESP_LOGW(TAG, "cccd write rc=%d", rc);
        }
    }
}

static void ble_host_task(void *param)
{
    nimble_port_run();
    nimble_port_freertos_deinit();
    vTaskDelete(NULL);
}

static void on_sync(void)
{
    // Use a public address type if available; NimBLE will pick.
    ble_hs_id_infer_auto(0, NULL);
}

bool chihiros_heater_client_init(chihiros_heater_client_t **out_client,
                                 const chihiros_heater_client_config_t *cfg,
                                 chihiros_heater_client_state_cb_t cb,
                                 void *cb_ctx)
{
    if (out_client == NULL) return false;

    static chihiros_heater_client_t s_client;  // single instance for now
    memset(&s_client, 0, sizeof(s_client));

    s_client.cfg.name_prefix = "DYH1";
    s_client.cfg.stale_timeout_ms = 10000;
    s_client.cfg.keepalive_enabled = false;
    s_client.cfg.keepalive_period_ms = 0;
    s_client.cfg.min_setpoint_f = 50.0f;
    s_client.cfg.max_setpoint_f = 95.0f;
    if (cfg) {
        s_client.cfg = *cfg;
        if (s_client.cfg.name_prefix == NULL) s_client.cfg.name_prefix = "DYH1";
        if (s_client.cfg.stale_timeout_ms == 0) s_client.cfg.stale_timeout_ms = 10000;
        if (!(s_client.cfg.min_setpoint_f < s_client.cfg.max_setpoint_f)) {
            s_client.cfg.min_setpoint_f = 50.0f;
            s_client.cfg.max_setpoint_f = 95.0f;
        }
    }

    s_client.cb = cb;
    s_client.cb_ctx = cb_ctx;
    s_client.conn_handle = BLE_HS_CONN_HANDLE_NONE;
    s_client.backoff_ms = 500;

    nimble_port_init();
    ble_hs_cfg.sync_cb = on_sync;
    nimble_port_freertos_init(ble_host_task);

    *out_client = &s_client;
    return true;
}

void chihiros_heater_client_request_connect(chihiros_heater_client_t *client)
{
    if (!client) return;
    client->connect_requested = true;
}

void chihiros_heater_client_request_disconnect(chihiros_heater_client_t *client)
{
    if (!client) return;
    client->disconnect_requested = true;
}

static void pump(chihiros_heater_client_t *c)
{
    uint64_t now_us = esp_timer_get_time();

    // Stale timeout
    if (c->state.connected && c->state.last_status.valid) {
        uint32_t age = now_ms() - c->state.last_status_ms;
        bool stale = age > c->cfg.stale_timeout_ms;
        if (stale != c->state.stale) {
            c->state.stale = stale;
            emit_state(c);
        }
    } else {
        c->state.stale = true;
    }

    if (c->disconnect_requested) {
        c->disconnect_requested = false;
        if (c->state.connected) {
            ESP_LOGI(TAG, "disconnect requested");
            ble_gap_terminate(c->conn_handle, BLE_ERR_REM_USER_CONN_TERM);
        }
        return;
    }

    if (!c->connect_requested) {
        return;
    }

    if (now_us < c->next_action_us) {
        return;
    }

    if (c->state.connected) {
        maybe_finish_discovery(c);
        return;
    }

    if (!c->have_adv_target) {
        int rc = scan_start(c);
        if (rc != 0) {
            ESP_LOGW(TAG, "scan_start rc=%d", rc);
        }
        c->next_action_us = now_us + (uint64_t)c->backoff_ms * 1000ULL;
        return;
    }

    int rc = connect_to_found(c);
    if (rc != 0) {
        ESP_LOGW(TAG, "connect rc=%d", rc);
        c->have_adv_target = false;
        if (c->backoff_ms < 30000) c->backoff_ms *= 2;
    } else {
        c->backoff_ms = 500;
    }
    c->next_action_us = now_us + (uint64_t)c->backoff_ms * 1000ULL;
}

bool chihiros_heater_client_set_setpoint_f(chihiros_heater_client_t *client, float target_f)
{
    if (!client) return false;
    if (!client->state.connected || client->nus_rx_handle == 0) {
        return false;
    }

    uint8_t pkt[11] = {0};
    if (!chihiros_make_setpoint_packet_f(target_f,
                                         client->cfg.min_setpoint_f,
                                         client->cfg.max_setpoint_f,
                                         pkt)) {
        return false;
    }

    int rc = write_no_rsp(pkt, sizeof(pkt), client);
    if (rc != 0) {
        ESP_LOGW(TAG, "setpoint write rc=%d", rc);
        return false;
    }

    client->state.last_setpoint_f = target_f;
    client->state.has_last_setpoint = true;
    emit_state(client);
    return true;
}

chihiros_heater_client_state_t chihiros_heater_client_get_state(chihiros_heater_client_t *client)
{
    if (!client) {
        chihiros_heater_client_state_t zero = {0};
        return zero;
    }
    return client->state;
}

// Simple task that drives connect/discovery/stale tracking.
static void client_task(void *arg)
{
    chihiros_heater_client_t *c = (chihiros_heater_client_t *)arg;
    while (true) {
        pump(c);
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

// Exposed helper for main to start the pump task.
void chihiros_heater_client_start_task(chihiros_heater_client_t *client)
{
    xTaskCreate(client_task, "chihiros_pump", 4096, client, 5, NULL);
}


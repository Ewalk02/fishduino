#include "chihiros_ble_client.h"

#include <string.h>

#include "ble/ble_central_manager.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "host/ble_gap.h"
#include "host/ble_gatt.h"
#include "host/ble_hs.h"
#include "host/ble_uuid.h"

static const char *TAG = "chihiros_ble";

static const ble_uuid128_t NUS_SVC_UUID =
    BLE_UUID128_INIT(0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa0, 0xe0, 0x93, 0xf3, 0xa3, 0xb5, 0x01, 0x00, 0x40, 0x6e);
static const ble_uuid128_t NUS_RX_UUID =
    BLE_UUID128_INIT(0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa0, 0xe0, 0x93, 0xf3, 0xa3, 0xb5, 0x02, 0x00, 0x40, 0x6e);
static const ble_uuid128_t NUS_TX_UUID =
    BLE_UUID128_INIT(0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa0, 0xe0, 0x93, 0xf3, 0xa3, 0xb5, 0x03, 0x00, 0x40, 0x6e);

static const uint8_t INIT_1[] = {0x5a, 0x01, 0x06, 0x00, 0x02, 0x04, 0x01, 0x00};
static const uint8_t INIT_2[] = {0x5a, 0x01, 0x0b, 0x00, 0x03, 0x09, 0x1a, 0x05, 0x02, 0x13, 0x25, 0x0c, 0x27};
static const uint8_t INIT_3[] = {0x5a, 0x01, 0x06, 0x00, 0x04, 0x04, 0x01, 0x06};

static chihiros_ble_client_config_t s_cfg;
static chihiros_ble_client_state_t s_state;
static SemaphoreHandle_t s_mutex;
static bool s_inited;
static bool s_enabled;
static bool s_connect_requested;

static uint16_t s_conn_handle;
static uint16_t s_nus_rx_handle;
static uint16_t s_nus_tx_handle;
static uint16_t s_nus_tx_cccd_handle;

static char s_adv_name[32];
static ble_addr_t s_adv_addr;
static bool s_have_adv_target;
static uint32_t s_backoff_ms;
static uint64_t s_next_action_us;

static uint32_t now_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000ULL);
}

static void state_lock(void)
{
    if (s_mutex) {
        xSemaphoreTake(s_mutex, portMAX_DELAY);
    }
}

static void state_unlock(void)
{
    if (s_mutex) {
        xSemaphoreGive(s_mutex);
    }
}

static bool name_has_prefix(const char *name, const char *prefix)
{
    if (name == NULL || prefix == NULL || prefix[0] == '\0') {
        return false;
    }
    return strncmp(name, prefix, strlen(prefix)) == 0;
}

static int write_no_rsp(const uint8_t *data, size_t len)
{
    if (!s_state.connected || s_nus_rx_handle == 0) {
        return -1;
    }
    struct os_mbuf *om = ble_hs_mbuf_from_flat(data, len);
    if (!om) {
        return -1;
    }
    return ble_gattc_write_no_rsp(s_conn_handle, s_nus_rx_handle, om);
}

static int send_init_sequence(void)
{
    int rc = write_no_rsp(INIT_1, sizeof(INIT_1));
    if (rc) {
        ESP_LOGW(TAG, "init1 rc=%d", rc);
    }
    rc = write_no_rsp(INIT_2, sizeof(INIT_2));
    if (rc) {
        ESP_LOGW(TAG, "init2 rc=%d", rc);
    }
    rc = write_no_rsp(INIT_3, sizeof(INIT_3));
    if (rc) {
        ESP_LOGW(TAG, "init3 rc=%d", rc);
    }
    return 0;
}

static void handle_notify(struct os_mbuf *om)
{
    if (om == NULL) {
        return;
    }

    uint8_t buf[64] = {0};
    int len = ble_hs_mbuf_to_flat(om, buf, sizeof(buf), NULL);
    if (len <= 0) {
        return;
    }

    chihiros_status_t st = {0};
    if (chihiros_decode_status_packet(buf, (size_t)len, &st)) {
        state_lock();
        s_state.last_status = st;
        s_state.last_status_ms = now_ms();
        s_state.stale = false;
        state_unlock();
    }
}

static int subscribe_complete_cb(uint16_t conn_handle, const struct ble_gatt_error *error,
                                 struct ble_gatt_attr *attr, void *arg)
{
    (void)conn_handle;
    (void)attr;
    (void)arg;

    if (error && error->status != 0) {
        ESP_LOGW(TAG, "CCCD failed=%d", error->status);
        return 0;
    }

    state_lock();
    s_state.subscribed = true;
    state_unlock();
    send_init_sequence();
    return 0;
}

static int discover_dsc_cb(uint16_t conn_handle, const struct ble_gatt_error *error, uint16_t chr_val_handle,
                           const struct ble_gatt_dsc *dsc, void *arg)
{
    (void)conn_handle;
    (void)chr_val_handle;
    (void)arg;

    if (error && error->status != 0 && error->status != BLE_HS_EDONE) {
        return 0;
    }
    if (dsc == NULL) {
        return 0;
    }

    ble_uuid16_t cccd_uuid = BLE_UUID16_INIT(0x2902);
    if (ble_uuid_cmp(&dsc->uuid.u, &cccd_uuid.u) == 0) {
        s_nus_tx_cccd_handle = dsc->handle;
    }
    return 0;
}

static int discover_chr_cb(uint16_t conn_handle, const struct ble_gatt_error *error, const struct ble_gatt_chr *chr,
                           void *arg)
{
    (void)conn_handle;
    (void)arg;

    if (error && error->status != 0) {
        return 0;
    }
    if (chr == NULL) {
        return 0;
    }

    if (ble_uuid_cmp(&chr->uuid.u, &NUS_RX_UUID.u) == 0) {
        s_nus_rx_handle = chr->val_handle;
    } else if (ble_uuid_cmp(&chr->uuid.u, &NUS_TX_UUID.u) == 0) {
        s_nus_tx_handle = chr->val_handle;
    }
    return 0;
}

static int discover_svc_cb(uint16_t conn_handle, const struct ble_gatt_error *error, const struct ble_gatt_svc *svc,
                           void *arg)
{
    (void)arg;

    if (error && error->status != 0) {
        return 0;
    }
    if (svc == NULL) {
        return 0;
    }

    if (ble_uuid_cmp(&svc->uuid.u, &NUS_SVC_UUID.u) == 0) {
        ble_gattc_disc_all_chrs(conn_handle, svc->start_handle, svc->end_handle, discover_chr_cb, NULL);
    }
    return 0;
}

static void maybe_finish_discovery(void)
{
    if (s_nus_rx_handle && s_nus_tx_handle && s_nus_tx_cccd_handle == 0) {
        ble_gattc_disc_all_dscs(s_conn_handle, s_nus_tx_handle, s_nus_tx_handle + 8, discover_dsc_cb, NULL);
    }

    if (s_nus_tx_cccd_handle != 0 && !s_state.subscribed) {
        uint8_t cccd_val[2] = {0x01, 0x00};
        ble_gattc_write_flat(s_conn_handle, s_nus_tx_cccd_handle, cccd_val, sizeof(cccd_val), subscribe_complete_cb,
                             NULL);
    }
}

static int chihiros_gap_event(struct ble_gap_event *event, void *arg)
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

        char tmp[sizeof(s_adv_name)] = {0};
        size_t copy = fields.name_len < sizeof(tmp) - 1 ? (size_t)fields.name_len : sizeof(tmp) - 1;
        memcpy(tmp, fields.name, copy);

        if (!name_has_prefix(tmp, s_cfg.name_prefix)) {
            return 0;
        }

        strncpy(s_adv_name, tmp, sizeof(s_adv_name) - 1);
        s_adv_addr = event->disc.addr;
        s_have_adv_target = true;
        ble_gap_disc_cancel();
        return 0;
    }

    case BLE_GAP_EVENT_CONNECT:
        if (event->connect.status != 0) {
            state_lock();
            s_state.connected = false;
            s_state.subscribed = false;
            state_unlock();
            s_conn_handle = BLE_HS_CONN_HANDLE_NONE;
            s_have_adv_target = false;
            if (s_backoff_ms < 30000) {
                s_backoff_ms *= 2;
            }
            return 0;
        }

        s_conn_handle = event->connect.conn_handle;
        state_lock();
        s_state.connected = true;
        s_state.subscribed = false;
        state_unlock();
        s_nus_rx_handle = 0;
        s_nus_tx_handle = 0;
        s_nus_tx_cccd_handle = 0;
        s_backoff_ms = 500;
        ble_gattc_disc_all_svcs(s_conn_handle, discover_svc_cb, NULL);
        return 0;

    case BLE_GAP_EVENT_DISCONNECT:
        state_lock();
        s_state.connected = false;
        s_state.subscribed = false;
        state_unlock();
        s_conn_handle = BLE_HS_CONN_HANDLE_NONE;
        s_nus_rx_handle = 0;
        s_nus_tx_handle = 0;
        s_nus_tx_cccd_handle = 0;
        s_have_adv_target = false;
        if (s_backoff_ms < 30000) {
            s_backoff_ms *= 2;
        }
        s_next_action_us = esp_timer_get_time() + (uint64_t)s_backoff_ms * 1000ULL;
        return 0;

    case BLE_GAP_EVENT_NOTIFY_RX:
        handle_notify(event->notify_rx.om);
        return 0;

    default:
        return 0;
    }
}

static int scan_start(void)
{
    if (!ble_central_manager_request_session(BLE_CENTRAL_DRV_CHIHIROS)) {
        return BLE_HS_EBUSY;
    }

    struct ble_gap_disc_params params = {0};
    params.passive = 0;
    params.itvl = 0x0010;
    params.window = 0x0010;
    params.filter_duplicates = 1;

    s_have_adv_target = false;
    return ble_gap_disc(BLE_OWN_ADDR_PUBLIC, BLE_HS_FOREVER, &params, ble_central_manager_gap_event, NULL);
}

static int connect_to_found(void)
{
    if (!s_have_adv_target) {
        return 0;
    }

    struct ble_gap_conn_params conn_params = {0};
    conn_params.scan_itvl = 0x0010;
    conn_params.scan_window = 0x0010;
    conn_params.itvl_min = 0x0010;
    conn_params.itvl_max = 0x0020;
    conn_params.latency = 0;
    conn_params.supervision_timeout = 0x0100;
    conn_params.min_ce_len = 0x0010;
    conn_params.max_ce_len = 0x0030;

    return ble_gap_connect(BLE_OWN_ADDR_PUBLIC, &s_adv_addr, 30000, &conn_params, ble_central_manager_gap_event,
                           NULL);
}

static void chihiros_ble_tick_fn(void *arg)
{
    (void)arg;

    if (!s_inited || !s_enabled || !s_connect_requested) {
        return;
    }

    state_lock();
    if (s_state.connected && s_state.last_status.valid) {
        uint32_t age = now_ms() - s_state.last_status_ms;
        bool stale = age > s_cfg.stale_timeout_ms;
        if (stale != s_state.stale) {
            s_state.stale = stale;
        }
    } else if (s_state.connected) {
        s_state.stale = true;
    }
    bool connected = s_state.connected;
    state_unlock();

    uint64_t now_us = esp_timer_get_time();
    if (now_us < s_next_action_us) {
        if (connected) {
            maybe_finish_discovery();
        }
        return;
    }

    if (connected) {
        maybe_finish_discovery();
        return;
    }

    if (!s_have_adv_target) {
        int rc = scan_start();
        if (rc != 0) {
            ESP_LOGW(TAG, "scan rc=%d", rc);
        }
        s_next_action_us = now_us + (uint64_t)s_backoff_ms * 1000ULL;
        return;
    }

    int rc = connect_to_found();
    if (rc != 0) {
        s_have_adv_target = false;
        if (s_backoff_ms < 30000) {
            s_backoff_ms *= 2;
        }
    }
    s_next_action_us = now_us + (uint64_t)s_backoff_ms * 1000ULL;
}

static void chihiros_on_sync(void *arg)
{
    (void)arg;
    s_connect_requested = true;
    s_next_action_us = esp_timer_get_time();
}

esp_err_t chihiros_ble_client_init(void)
{
    if (s_inited) {
        return ESP_OK;
    }

    memset(&s_cfg, 0, sizeof(s_cfg));
    snprintf(s_cfg.name_prefix, sizeof(s_cfg.name_prefix), "DYH1");
    s_cfg.stale_timeout_ms = 10000;
    s_cfg.min_setpoint_f = 50.0f;
    s_cfg.max_setpoint_f = 95.0f;

    s_mutex = xSemaphoreCreateMutex();
    if (!s_mutex) {
        return ESP_ERR_NO_MEM;
    }

    esp_err_t err = ble_central_manager_init();
    if (err != ESP_OK) {
        return err;
    }

    static bool registered;
    if (!registered) {
        ble_central_driver_reg_t reg = {
            .name = "chihiros",
            .priority = BLE_CENTRAL_PRIO_CHIHIROS,
            .enabled = false,
            .gap_handler = chihiros_gap_event,
            .gap_arg = NULL,
            .tick_fn = chihiros_ble_tick_fn,
            .tick_arg = NULL,
            .on_sync_fn = chihiros_on_sync,
            .on_sync_arg = NULL,
        };
        err = ble_central_manager_register(BLE_CENTRAL_DRV_CHIHIROS, &reg);
        if (err != ESP_OK) {
            return err;
        }
        registered = true;
    }

    s_inited = true;
    return ESP_OK;
}

esp_err_t chihiros_ble_client_set_config(const chihiros_ble_client_config_t *cfg)
{
    if (cfg == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    s_cfg = *cfg;
    if (s_cfg.name_prefix[0] == '\0') {
        snprintf(s_cfg.name_prefix, sizeof(s_cfg.name_prefix), "DYH1");
    }
    if (s_cfg.stale_timeout_ms == 0) {
        s_cfg.stale_timeout_ms = 10000;
    }
    return ESP_OK;
}

esp_err_t chihiros_ble_client_set_enabled(bool enabled)
{
    s_enabled = enabled;
    ble_central_manager_set_driver_enabled(BLE_CENTRAL_DRV_CHIHIROS, enabled);
    if (enabled) {
        chihiros_ble_client_request_connect();
    }
    return ESP_OK;
}

void chihiros_ble_client_request_connect(void)
{
    s_connect_requested = true;
}

bool chihiros_ble_client_is_ready(void)
{
    return s_inited && s_state.connected && s_state.subscribed;
}

esp_err_t chihiros_ble_client_get_state(chihiros_ble_client_state_t *out)
{
    if (out == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    state_lock();
    *out = s_state;
    state_unlock();
    return ESP_OK;
}

bool chihiros_ble_client_set_setpoint_f(float target_f)
{
    if (!s_state.connected || s_nus_rx_handle == 0) {
        return false;
    }

    uint8_t pkt[11] = {0};
    if (!chihiros_make_setpoint_packet_f(target_f, s_cfg.min_setpoint_f, s_cfg.max_setpoint_f, pkt)) {
        return false;
    }

    if (write_no_rsp(pkt, sizeof(pkt)) != 0) {
        return false;
    }

    state_lock();
    s_state.last_setpoint_f = target_f;
    s_state.has_last_setpoint = true;
    state_unlock();
    return true;
}

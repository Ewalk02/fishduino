#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "host/ble_gap.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BLE_CENTRAL_DRV_FLUVAL    0
#define BLE_CENTRAL_DRV_CHIHIROS  1
#define BLE_CENTRAL_DRV_COUNT     2

/** Higher value = higher priority for connection ownership. */
#define BLE_CENTRAL_PRIO_CHIHIROS 20
#define BLE_CENTRAL_PRIO_FLUVAL    10

typedef int (*ble_central_gap_handler_t)(struct ble_gap_event *event, void *arg);

typedef struct {
    const char *name;
    uint8_t priority;
    bool enabled;
    ble_central_gap_handler_t gap_handler;
    void *gap_arg;
    /** Called from central tick when this driver may scan/connect. */
    void (*tick_fn)(void *arg);
    void *tick_arg;
    /** Called once when NimBLE host sync completes. */
    void (*on_sync_fn)(void *arg);
    void *on_sync_arg;
} ble_central_driver_reg_t;

esp_err_t ble_central_manager_init(void);
esp_err_t ble_central_manager_register(uint8_t driver_id, const ble_central_driver_reg_t *reg);

void ble_central_manager_set_driver_enabled(uint8_t driver_id, bool enabled);

/** True if NimBLE host is synced and ready. */
bool ble_central_manager_is_ready(void);

/** Current connection owner, or BLE_CENTRAL_DRV_COUNT if none. */
uint8_t ble_central_manager_connection_owner(void);

/**
 * Request exclusive BLE access for scan/connect.
 * Returns false if a higher-priority driver holds the link.
 */
bool ble_central_manager_request_session(uint8_t driver_id);

/** Release session when idle (not connected). */
void ble_central_manager_release_session(uint8_t driver_id);

/**
 * Notify manager that driver established GATT connection.
 * Terminates other connections if needed.
 */
void ble_central_manager_claim_connection(uint8_t driver_id, uint16_t conn_handle);

void ble_central_manager_clear_connection(uint8_t driver_id);

/** Force disconnect current connection (any owner). */
void ble_central_manager_disconnect_active(void);

/** Run arbitration + driver tick functions (call from scheduler). */
void ble_central_manager_tick(void);

/** Multiplexed GAP handler — drivers must not call ble_gap_disc/connect directly without session. */
int ble_central_manager_gap_event(struct ble_gap_event *event, void *arg);

#ifdef __cplusplus
}
#endif

#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*fishduino_fluval_transport_line_cb_t)(const char *line, void *ctx);

typedef struct {
    esp_err_t (*init)(void *ctx);
    esp_err_t (*start)(void *ctx);
    esp_err_t (*stop)(void *ctx);
    esp_err_t (*send_line)(void *ctx, const char *line);
    bool (*is_active)(void *ctx);
} fishduino_fluval_transport_ops_t;

typedef struct {
    const fishduino_fluval_transport_ops_t *ops;
    void *ctx;
} fishduino_fluval_transport_t;

esp_err_t fishduino_fluval_transport_init(fishduino_fluval_transport_t *transport,
                                          fishduino_fluval_transport_line_cb_t line_cb, void *line_ctx);
esp_err_t fishduino_fluval_transport_start(fishduino_fluval_transport_t *transport);
esp_err_t fishduino_fluval_transport_stop(fishduino_fluval_transport_t *transport);
esp_err_t fishduino_fluval_transport_send_line(fishduino_fluval_transport_t *transport, const char *line);
bool fishduino_fluval_transport_is_active(fishduino_fluval_transport_t *transport);

const fishduino_fluval_transport_ops_t *fishduino_fluval_stub_transport_ops(void);
const fishduino_fluval_transport_ops_t *fishduino_fluval_uart_transport_ops(void);
const fishduino_fluval_transport_ops_t *fishduino_fluval_ble_transport_ops(void);

#ifdef __cplusplus
}
#endif

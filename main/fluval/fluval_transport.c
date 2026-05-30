#include "fluval_transport.h"

#include "esp_err.h"

esp_err_t fishduino_fluval_transport_init(fishduino_fluval_transport_t *transport,
                                          fishduino_fluval_transport_line_cb_t line_cb, void *line_ctx)
{
    if (transport == NULL || transport->ops == NULL || transport->ops->init == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = transport->ops->init(transport->ctx);
    if (err != ESP_OK) {
        return err;
    }

    if (transport->ops == fishduino_fluval_uart_transport_ops()) {
        extern esp_err_t fishduino_fluval_uart_transport_set_callback(fishduino_fluval_transport_line_cb_t cb,
                                                                      void *ctx);
        return fishduino_fluval_uart_transport_set_callback(line_cb, line_ctx);
    }

    (void)line_cb;
    (void)line_ctx;
    return ESP_OK;
}

esp_err_t fishduino_fluval_transport_start(fishduino_fluval_transport_t *transport)
{
    if (transport == NULL || transport->ops == NULL || transport->ops->start == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    return transport->ops->start(transport->ctx);
}

esp_err_t fishduino_fluval_transport_stop(fishduino_fluval_transport_t *transport)
{
    if (transport == NULL || transport->ops == NULL || transport->ops->stop == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    return transport->ops->stop(transport->ctx);
}

esp_err_t fishduino_fluval_transport_send_line(fishduino_fluval_transport_t *transport, const char *line)
{
    if (transport == NULL || transport->ops == NULL || transport->ops->send_line == NULL || line == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    return transport->ops->send_line(transport->ctx, line);
}

bool fishduino_fluval_transport_is_active(fishduino_fluval_transport_t *transport)
{
    if (transport == NULL || transport->ops == NULL || transport->ops->is_active == NULL) {
        return false;
    }
    return transport->ops->is_active(transport->ctx);
}

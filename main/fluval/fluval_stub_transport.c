#include "fluval_transport.h"

static esp_err_t stub_init(void *ctx)
{
    (void)ctx;
    return ESP_OK;
}

static esp_err_t stub_start(void *ctx)
{
    (void)ctx;
    return ESP_OK;
}

static esp_err_t stub_stop(void *ctx)
{
    (void)ctx;
    return ESP_OK;
}

static esp_err_t stub_send_line(void *ctx, const char *line)
{
    (void)ctx;
    (void)line;
    return ESP_ERR_INVALID_STATE;
}

static bool stub_is_active(void *ctx)
{
    (void)ctx;
    return false;
}

static const fishduino_fluval_transport_ops_t s_stub_ops = {
    .init = stub_init,
    .start = stub_start,
    .stop = stub_stop,
    .send_line = stub_send_line,
    .is_active = stub_is_active,
};

const fishduino_fluval_transport_ops_t *fishduino_fluval_stub_transport_ops(void)
{
    return &s_stub_ops;
}

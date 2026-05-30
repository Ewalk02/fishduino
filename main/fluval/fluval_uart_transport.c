#include "fluval_transport.h"

#include <string.h>

#include "driver/uart.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "hardware_pins.h"

static const char *TAG = "fluval_uart";

#define FLUVAL_UART_RX_BUF_SIZE 512
#define FLUVAL_UART_TX_BUF_SIZE 256
#define FLUVAL_UART_LINE_MAX    192

typedef struct {
    bool configured;
    bool started;
    fishduino_fluval_transport_line_cb_t line_cb;
    void *line_ctx;
    TaskHandle_t rx_task;
    char line_buf[FLUVAL_UART_LINE_MAX];
    size_t line_len;
} fluval_uart_ctx_t;

static fluval_uart_ctx_t s_uart_ctx;

static bool uart_pins_configured(void)
{
    return FISHDUINO_FLUVAL_UART_NUM >= 0 && FISHDUINO_FLUVAL_UART_TX >= 0 && FISHDUINO_FLUVAL_UART_RX >= 0;
}

esp_err_t fishduino_fluval_uart_transport_set_callback(fishduino_fluval_transport_line_cb_t cb, void *ctx)
{
    s_uart_ctx.line_cb = cb;
    s_uart_ctx.line_ctx = ctx;
    return ESP_OK;
}

static void dispatch_line(const char *line)
{
    if (line == NULL || line[0] == '\0' || s_uart_ctx.line_cb == NULL) {
        return;
    }
    s_uart_ctx.line_cb(line, s_uart_ctx.line_ctx);
}

static void append_rx_byte(uint8_t byte)
{
    if (byte == '\r') {
        return;
    }
    if (byte == '\n') {
        if (s_uart_ctx.line_len > 0) {
            s_uart_ctx.line_buf[s_uart_ctx.line_len] = '\0';
            dispatch_line(s_uart_ctx.line_buf);
            s_uart_ctx.line_len = 0;
        }
        return;
    }

    if (s_uart_ctx.line_len + 1 >= sizeof(s_uart_ctx.line_buf)) {
        s_uart_ctx.line_len = 0;
        return;
    }

    s_uart_ctx.line_buf[s_uart_ctx.line_len++] = (char)byte;
}

static void uart_rx_task(void *arg)
{
    (void)arg;
    uint8_t chunk[64];

    while (s_uart_ctx.started) {
        int n = uart_read_bytes((uart_port_t)FISHDUINO_FLUVAL_UART_NUM, chunk, sizeof(chunk),
                                pdMS_TO_TICKS(100));
        if (n <= 0) {
            continue;
        }
        for (int i = 0; i < n; i++) {
            append_rx_byte(chunk[i]);
        }
    }
    s_uart_ctx.rx_task = NULL;
    vTaskDelete(NULL);
}

static esp_err_t uart_init(void *ctx)
{
    (void)ctx;
    memset(&s_uart_ctx, 0, sizeof(s_uart_ctx));
    s_uart_ctx.configured = uart_pins_configured();
    if (!s_uart_ctx.configured) {
        ESP_LOGI(TAG, "Fluval UART not configured (pins unset)");
        return ESP_OK;
    }

    const uart_config_t cfg = {
        .baud_rate = FISHDUINO_FLUVAL_UART_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    esp_err_t err = uart_driver_install((uart_port_t)FISHDUINO_FLUVAL_UART_NUM, FLUVAL_UART_RX_BUF_SIZE,
                                        FLUVAL_UART_TX_BUF_SIZE, 0, NULL, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "uart_driver_install failed: %s", esp_err_to_name(err));
        s_uart_ctx.configured = false;
        return err;
    }

    err = uart_param_config((uart_port_t)FISHDUINO_FLUVAL_UART_NUM, &cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "uart_param_config failed: %s", esp_err_to_name(err));
        uart_driver_delete((uart_port_t)FISHDUINO_FLUVAL_UART_NUM);
        s_uart_ctx.configured = false;
        return err;
    }

    err = uart_set_pin((uart_port_t)FISHDUINO_FLUVAL_UART_NUM, FISHDUINO_FLUVAL_UART_TX,
                       FISHDUINO_FLUVAL_UART_RX, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "uart_set_pin failed: %s", esp_err_to_name(err));
        uart_driver_delete((uart_port_t)FISHDUINO_FLUVAL_UART_NUM);
        s_uart_ctx.configured = false;
        return err;
    }

    ESP_LOGI(TAG, "Fluval UART ready on port %d", FISHDUINO_FLUVAL_UART_NUM);
    return ESP_OK;
}

static esp_err_t uart_start(void *ctx)
{
    (void)ctx;
    if (!s_uart_ctx.configured || s_uart_ctx.started) {
        return ESP_OK;
    }

    s_uart_ctx.started = true;
    BaseType_t ok = xTaskCreate(uart_rx_task, "fluval_uart_rx", 3072, NULL, 4, &s_uart_ctx.rx_task);
    if (ok != pdPASS) {
        s_uart_ctx.started = false;
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

static esp_err_t uart_stop(void *ctx)
{
    (void)ctx;
    if (!s_uart_ctx.started) {
        return ESP_OK;
    }

    s_uart_ctx.started = false;
    while (s_uart_ctx.rx_task != NULL) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    if (s_uart_ctx.configured) {
        uart_driver_delete((uart_port_t)FISHDUINO_FLUVAL_UART_NUM);
        s_uart_ctx.configured = false;
    }
    return ESP_OK;
}

static esp_err_t uart_send_line(void *ctx, const char *line)
{
    (void)ctx;
    if (!s_uart_ctx.configured || line == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    char buf[FLUVAL_UART_LINE_MAX + 2];
    int n = snprintf(buf, sizeof(buf), "%s\n", line);
    if (n <= 0 || n >= (int)sizeof(buf)) {
        return ESP_ERR_INVALID_ARG;
    }

    int written = uart_write_bytes((uart_port_t)FISHDUINO_FLUVAL_UART_NUM, buf, (size_t)n);
    if (written != n) {
        return ESP_FAIL;
    }
    return ESP_OK;
}

static bool uart_is_active(void *ctx)
{
    (void)ctx;
    return s_uart_ctx.configured && s_uart_ctx.started;
}

static const fishduino_fluval_transport_ops_t s_uart_ops = {
    .init = uart_init,
    .start = uart_start,
    .stop = uart_stop,
    .send_line = uart_send_line,
    .is_active = uart_is_active,
};

const fishduino_fluval_transport_ops_t *fishduino_fluval_uart_transport_ops(void)
{
    return &s_uart_ops;
}

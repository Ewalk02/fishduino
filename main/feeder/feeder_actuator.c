#include "feeder_actuator.h"

#include "hardware_pins.h"

#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"

static const char *TAG = "feeder_act";

static TimerHandle_t off_timer;

static void off_timer_cb(TimerHandle_t xTimer)
{
    (void)xTimer;
    if (FISHDUINO_GPIO_FEEDER_CTRL >= 0) {
        gpio_set_level(FISHDUINO_GPIO_FEEDER_CTRL, 0);
    }
}

bool fishduino_feeder_actuator_is_configured(void)
{
    return FISHDUINO_GPIO_FEEDER_CTRL >= 0;
}

void fishduino_feeder_actuator_init(void)
{
#if FISHDUINO_GPIO_FEEDER_CTRL >= 0
    gpio_config_t cfg = {
        .pin_bit_mask = 1ULL << FISHDUINO_GPIO_FEEDER_CTRL,
        .mode = GPIO_MODE_OUTPUT,
        .pull_down_en = 0,
        .pull_up_en = 0,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&cfg);
    gpio_set_level(FISHDUINO_GPIO_FEEDER_CTRL, 0);
#endif

    off_timer = xTimerCreate("feed_off", pdMS_TO_TICKS(1000), pdFALSE, NULL, off_timer_cb);
    if (off_timer == NULL) {
        ESP_LOGE(TAG, "Failed to create feeder off timer");
    }
}

void fishduino_feeder_actuator_pulse(uint32_t ms)
{
    if (!fishduino_feeder_actuator_is_configured() || off_timer == NULL) {
        return;
    }

    gpio_set_level(FISHDUINO_GPIO_FEEDER_CTRL, 1);
    xTimerStop(off_timer, 0);
    xTimerChangePeriod(off_timer, pdMS_TO_TICKS(ms), 0);
    xTimerStart(off_timer, 0);
}


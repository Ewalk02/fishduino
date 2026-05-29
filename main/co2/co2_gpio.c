#include "co2_gpio.h"

#include "hardware_pins.h"

#include "driver/gpio.h"

bool fishduino_co2_gpio_is_configured(void)
{
    return FISHDUINO_GPIO_CO2_RELAY >= 0;
}

void fishduino_co2_gpio_init(void)
{
#if FISHDUINO_GPIO_CO2_RELAY >= 0
    gpio_config_t cfg = {
        .pin_bit_mask = 1ULL << FISHDUINO_GPIO_CO2_RELAY,
        .mode = GPIO_MODE_OUTPUT,
        .pull_down_en = 0,
        .pull_up_en = 0,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&cfg);
    gpio_set_level(FISHDUINO_GPIO_CO2_RELAY, 0);
#endif
}

void fishduino_co2_gpio_set(bool on)
{
    if (!fishduino_co2_gpio_is_configured()) {
        return;
    }
    gpio_set_level(FISHDUINO_GPIO_CO2_RELAY, on ? 1 : 0);
}


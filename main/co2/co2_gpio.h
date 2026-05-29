#pragma once

#include <stdbool.h>

void fishduino_co2_gpio_init(void);
void fishduino_co2_gpio_set(bool on);
bool fishduino_co2_gpio_is_configured(void);


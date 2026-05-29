#pragma once

#include <stdbool.h>
#include <stdint.h>

void fishduino_feeder_actuator_init(void);
bool fishduino_feeder_actuator_is_configured(void);
void fishduino_feeder_actuator_pulse(uint32_t ms);


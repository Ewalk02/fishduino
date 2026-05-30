#pragma once

// Phase 1: Fill these in once you decide which 40-pin header pins
// you want to use for CO2 and feeder control.
//
// Use `-1` to disable an actuator at runtime until wiring is finalized.

#define FISHDUINO_GPIO_CO2_RELAY   (-1)
#define FISHDUINO_GPIO_FEEDER_CTRL (-1)

// Fluval BLE helper UART (separate from console UART). Use -1 until wired.
#define FISHDUINO_FLUVAL_UART_NUM  (-1)
#define FISHDUINO_FLUVAL_UART_TX   (-1)
#define FISHDUINO_FLUVAL_UART_RX   (-1)
#define FISHDUINO_FLUVAL_UART_BAUD  (115200)


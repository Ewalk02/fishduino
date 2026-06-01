#pragma once

// Waveshare ESP32-P4-WIFI6-7inch-Touch-LCD (B).
// Exposed GPIO for aquarium peripherals is not documented on the 7B schematic
// in-repo — do not guess assignments.

#define FISHDUINO_GPIO_CO2_RELAY   (-1) /* TODO: map when 7B header wiring is defined */
#define FISHDUINO_GPIO_FEEDER_CTRL (-1) /* TODO: map when 7B header wiring is defined */

#define FISHDUINO_FLUVAL_UART_NUM  (-1) /* TODO: external UART helper if used on 7B */
#define FISHDUINO_FLUVAL_UART_TX   (-1)
#define FISHDUINO_FLUVAL_UART_RX   (-1)
#define FISHDUINO_FLUVAL_UART_BAUD  (115200)

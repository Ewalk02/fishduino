#pragma once

// Default plug IPs (placeholders). For real IPs use shelly_config.local.h (gitignored):
//   cp main/shelly/shelly_config.local.h.example main/shelly/shelly_config.local.h
#if defined(__has_include) && __has_include("shelly_config.local.h")
#include "shelly_config.local.h"
#endif

#ifndef FISHDUINO_CO2_SHELLY_IP_DEFAULT
#define FISHDUINO_CO2_SHELLY_IP_DEFAULT    "192.168.1.XXX"
#endif
#ifndef FISHDUINO_FILTER_SHELLY_IP_DEFAULT
#define FISHDUINO_FILTER_SHELLY_IP_DEFAULT "192.168.1.YYY"
#endif
#ifndef FISHDUINO_HEATER_SHELLY_IP_DEFAULT
#define FISHDUINO_HEATER_SHELLY_IP_DEFAULT "192.168.1.ZZZ"
#endif

#define FISHDUINO_SHELLY_POLL_MS           5000
#define FISHDUINO_SHELLY_HTTP_TIMEOUT_MS   1500
#define FISHDUINO_SHELLY_FAIL_OFFLINE      3
#define FISHDUINO_SHELLY_CO2_CMD_MIN_MS     10000

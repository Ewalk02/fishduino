#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "storage/settings_nvs.h"

/** Valid if empty (when allow_empty) or IPv4 dotted-quad or LAN hostname. */
bool fishduino_shelly_address_valid(const char *addr, bool allow_empty);

/** Fill buf with a short error message; returns false if valid. */
bool fishduino_shelly_address_error(const char *addr, bool allow_empty, char *buf, size_t len);

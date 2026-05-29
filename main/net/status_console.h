#pragma once

#include <stddef.h>

void fishduino_status_console_register(void);

/** Format active settings into buf (for UI diagnostics). */
void fishduino_status_format(char *buf, size_t len);

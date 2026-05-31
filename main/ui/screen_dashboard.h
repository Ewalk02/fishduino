#pragma once

#include "screen_dashboard_cockpit.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef fishduino_cockpit_handles_t fishduino_dashboard_handles_t;

fishduino_dashboard_handles_t fishduino_screen_dashboard_build(lv_obj_t *parent);

#ifdef __cplusplus
}
#endif

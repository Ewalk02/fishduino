#include "co2_schedule.h"

#include <string.h>

#include "co2_gpio.h"
#include "safety/co2_safety.h"
#include "scheduler/scheduler.h"
#include "storage/settings_nvs.h"

bool fishduino_co2_in_schedule_window(uint16_t now_min, uint16_t on_min, uint16_t off_min)
{
    if (on_min == off_min) {
        return false;
    }
    if (on_min < off_min) {
        return (now_min >= on_min) && (now_min < off_min);
    }
    return (now_min >= on_min) || (now_min < off_min);
}

bool fishduino_co2_get_target(const fishduino_co2_t *co2, const fishduino_time_snapshot_t *now)
{
    if (co2->settings.co2.manual_override) {
        return co2->settings.co2.manual_on;
    }
    if (co2->settings.co2.enabled && now->valid_time) {
        return fishduino_co2_in_schedule_window(now->minutes_since_midnight, co2->settings.co2.on_min,
                                               co2->settings.co2.off_min);
    }
    return false;
}

void fishduino_co2_init(fishduino_co2_t *co2, const fishduino_settings_t *settings)
{
    memset(co2, 0, sizeof(*co2));
    co2->settings = *settings;
    co2->output_on = false;

    fishduino_co2_gpio_init();
    fishduino_co2_gpio_set(false);
}

void fishduino_co2_apply_settings(fishduino_co2_t *co2, const fishduino_settings_t *settings)
{
    co2->settings = *settings;
}

void fishduino_co2_tick(fishduino_co2_t *co2, const fishduino_time_snapshot_t *now)
{
    bool target = fishduino_co2_get_target(co2, now);
    co2_safety_reason_t reason = CO2_BLOCK_NONE;
    target = fishduino_co2_safety_effective_desired_on(target, &reason);
    (void)reason;

    if (co2->settings.shelly_co2.enabled) {
        co2->output_on = target;
        return;
    }

    if (target != co2->output_on) {
        co2->output_on = target;
        fishduino_co2_gpio_set(co2->output_on);
    }
}

bool fishduino_co2_get_output(const fishduino_co2_t *co2)
{
    return co2->output_on;
}

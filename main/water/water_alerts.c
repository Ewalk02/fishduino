#include "water_alerts.h"

#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

const char *water_alerts_level_text(water_alert_level_t level)
{
    switch (level) {
    case WATER_ALERT_NOTICE:
        return "notice";
    case WATER_ALERT_WARNING:
        return "warning";
    default:
        return "ok";
    }
}

static bool field_valid(const water_test_entry_t *e, uint8_t bit)
{
    return e != NULL && (e->valid_flags & bit) != 0;
}

water_alert_level_t water_alerts_classify(const water_test_entry_t *e, char *msg, size_t msg_len)
{
    if (e == NULL) {
        if (msg != NULL && msg_len > 0) {
            snprintf(msg, msg_len, "no data");
        }
        return WATER_ALERT_OK;
    }

    water_alert_level_t worst = WATER_ALERT_OK;
    char buf[128] = {0};

    if (field_valid(e, WATER_VALID_AMMONIA) && e->ammonia_ppm > 0.0f) {
        strncat(buf, "Ammonia>0 ", sizeof(buf) - strlen(buf) - 1);
        worst = WATER_ALERT_WARNING;
    }

    if (field_valid(e, WATER_VALID_NITRITE) && e->nitrite_ppm > 0.0f) {
        strncat(buf, "Nitrite>0 ", sizeof(buf) - strlen(buf) - 1);
        worst = WATER_ALERT_WARNING;
    }

    if (field_valid(e, WATER_VALID_NITRATE) && e->nitrate_ppm > WATER_ALERT_NITRATE_MAX_PPM) {
        strncat(buf, "Nitrate high ", sizeof(buf) - strlen(buf) - 1);
        worst = WATER_ALERT_WARNING;
    }

    if (field_valid(e, WATER_VALID_PH)) {
        if (e->ph < WATER_ALERT_PH_MIN_DEFAULT || e->ph > WATER_ALERT_PH_MAX_DEFAULT) {
            strncat(buf, "pH out of range ", sizeof(buf) - strlen(buf) - 1);
            if (worst < WATER_ALERT_NOTICE) {
                worst = WATER_ALERT_NOTICE;
            }
            if (e->ph < 6.0f || e->ph > 8.5f) {
                worst = WATER_ALERT_WARNING;
            }
        }
    }

    if (msg != NULL && msg_len > 0) {
        if (buf[0] != '\0') {
            snprintf(msg, msg_len, "%s", buf);
        } else {
            msg[0] = '\0';
        }
    }

    return worst;
}

#include "shelly_address.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

static bool is_empty(const char *addr)
{
    return addr == NULL || addr[0] == '\0';
}

static bool ipv4_valid(const char *addr)
{
    unsigned a, b, c, d;
    char tail = '\0';
    if (sscanf(addr, "%u.%u.%u.%u%c", &a, &b, &c, &d, &tail) != 4) {
        return false;
    }
    if (tail != '\0') {
        return false;
    }
    return a <= 255 && b <= 255 && c <= 255 && d <= 255;
}

static bool hostname_valid(const char *addr)
{
    size_t n = strlen(addr);
    if (n == 0 || n >= FISHDUINO_IP_LEN) {
        return false;
    }
    if (addr[0] == '-' || addr[0] == '.' || addr[n - 1] == '-' || addr[n - 1] == '.') {
        return false;
    }

    bool has_alnum = false;
    for (size_t i = 0; i < n; i++) {
        char ch = addr[i];
        if (isalnum((unsigned char)ch)) {
            has_alnum = true;
            continue;
        }
        if (ch == '.' || ch == '-') {
            continue;
        }
        return false;
    }
    return has_alnum;
}

bool fishduino_shelly_address_valid(const char *addr, bool allow_empty)
{
    if (is_empty(addr)) {
        return allow_empty;
    }
    if (ipv4_valid(addr)) {
        return true;
    }
    return hostname_valid(addr);
}

bool fishduino_shelly_address_error(const char *addr, bool allow_empty, char *buf, size_t len)
{
    if (buf == NULL || len == 0) {
        return !fishduino_shelly_address_valid(addr, allow_empty);
    }

    if (fishduino_shelly_address_valid(addr, allow_empty)) {
        buf[0] = '\0';
        return false;
    }

    if (is_empty(addr)) {
        snprintf(buf, len, "IP required when enabled");
        return true;
    }

    snprintf(buf, len, "Invalid IPv4 or hostname");
    return true;
}

static bool plug_ip_same(const fishduino_shelly_plug_settings_t *a, const fishduino_shelly_plug_settings_t *b)
{
    if (a == NULL || b == NULL) {
        return false;
    }
    if (a->ip[0] == '\0' || b->ip[0] == '\0') {
        return false;
    }
    return strncmp(a->ip, b->ip, FISHDUINO_IP_LEN) == 0;
}

static bool controllable_same_endpoint(const fishduino_shelly_plug_settings_t *a,
                                       const fishduino_shelly_plug_settings_t *b)
{
    return a->enabled && b->enabled && plug_ip_same(a, b) && a->switch_id == b->switch_id;
}

bool fishduino_shelly_plugs_config_error(const fishduino_settings_t *settings, char *buf, size_t len)
{
    if (settings == NULL) {
        return false;
    }

    const fishduino_shelly_plug_settings_t *co2 = &settings->shelly_co2;
    const fishduino_shelly_plug_settings_t *filter = &settings->shelly_filter;
    const fishduino_shelly_plug_settings_t *heater = &settings->shelly_heater;

    if (controllable_same_endpoint(co2, heater)) {
        snprintf(buf, len, "CO2 and heater share IP and switch id");
        return true;
    }
    if (co2->enabled && heater->enabled && plug_ip_same(co2, heater)) {
        snprintf(buf, len, "CO2 and heater share the same IP");
        return true;
    }
    if (co2->enabled && filter->enabled && plug_ip_same(co2, filter)) {
        snprintf(buf, len, "CO2 and filter share the same IP");
        return true;
    }
    if (heater->enabled && filter->enabled && plug_ip_same(heater, filter)) {
        snprintf(buf, len, "Heater and filter share the same IP");
        return true;
    }

    if (buf != NULL && len > 0) {
        buf[0] = '\0';
    }
    return false;
}

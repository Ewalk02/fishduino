#include "chihiros_heater_protocol.h"

#include <math.h>
#include <string.h>

static bool bytes_eq(const uint8_t *a, const uint8_t *b, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        if (a[i] != b[i]) return false;
    }
    return true;
}

static bool float_close(float a, float b, float eps)
{
    return fabsf(a - b) <= eps;
}

bool chihiros_protocol_run_selftests(void)
{
    // Acceptance vectors from reverse engineering summary.
    struct {
        float f;
        uint8_t expected[11];
    } cases[] = {
        {77.0f, {0x5a, 0x01, 0x09, 0x00, 0x05, 0x2b, 0x01, 0x19, 0x06, 0x32, 0x0a}},
        {77.5f, {0x5a, 0x01, 0x09, 0x00, 0x05, 0x2b, 0x01, 0x19, 0x20, 0x32, 0x2c}},
        {78.5f, {0x5a, 0x01, 0x09, 0x00, 0x05, 0x2b, 0x01, 0x19, 0x55, 0x32, 0x59}},
        {78.8f, {0x5a, 0x01, 0x09, 0x00, 0x05, 0x2b, 0x01, 0x1a, 0x06, 0x32, 0x09}},
    };

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        uint8_t pkt[11] = {0};
        if (!chihiros_make_setpoint_packet_f(cases[i].f, 50.0f, 95.0f, pkt)) {
            return false;
        }
        if (!bytes_eq(pkt, cases[i].expected, sizeof(pkt))) {
            return false;
        }
    }

    // Checksum vectors (checksum computed over first 10 bytes).
    {
        uint8_t pkt10[] = {0x5a, 0x01, 0x09, 0x00, 0x05, 0x2b, 0x01, 0x19, 0x06, 0x32};
        if (chihiros_checksum_xor(pkt10, sizeof(pkt10)) != 0x0a) return false;
    }
    {
        uint8_t pkt10[] = {0x5a, 0x01, 0x09, 0x00, 0x05, 0x2b, 0x01, 0x19, 0x55, 0x32};
        if (chihiros_checksum_xor(pkt10, sizeof(pkt10)) != 0x59) return false;
    }

    // Status decode vector.
    {
        uint8_t status[] = {0x5b, 0x03, 0x0e, 0x00, 0x25, 0x25, 0x02, 0x72, 0x0b, 0xff, 0x00, 0xfe, 0x00};
        chihiros_status_t st = {0};
        if (!chihiros_decode_status_packet(status, sizeof(status), &st)) return false;
        if (!st.valid) return false;
        if (st.watts != 626) return false;
        if (!st.heating) return false;
        if (!float_close(st.current_temp_c, 25.4f, 0.01f)) return false;
        if (!float_close(st.current_temp_f, 77.72f, 0.05f)) return false;
    }

    // Invalid-temp packet should be rejected.
    {
        uint8_t status[] = {0x5b, 0x03, 0x0e, 0x00, 0x01, 0x0a, 0x00, 0x00, 0x0f, 0xff, 0xff, 0xff, 0x22, 0x05, 0x00, 0x22};
        chihiros_status_t st = {0};
        if (chihiros_decode_status_packet(status, sizeof(status), &st)) return false;
    }

    return true;
}


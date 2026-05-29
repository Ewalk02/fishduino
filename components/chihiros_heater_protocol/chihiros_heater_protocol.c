#include "chihiros_heater_protocol.h"

#include <math.h>
#include <string.h>

// Reverse engineered constants
static const uint8_t SETPOINT_SEQ = 0x05;
static const uint8_t SETPOINT_CMD = 0x2b;
static const uint8_t SETPOINT_CONST_1 = 0x01;
static const uint8_t SETPOINT_CONST_2 = 0x32;

// Fine mapping (linear approximation) inside a whole-C block.
// Calibrated around WHOLE_C=0x19 from reverse engineering doc.
static const float FINE_AT_BLOCK_START = 0x06;
static const float FINE_COUNTS_PER_F = (float)(0x2B - 0x06) / 0.7f;  // ≈ 52.857

static float clampf(float x, float lo, float hi)
{
    if (x < lo) return lo;
    if (x > hi) return hi;
    return x;
}

uint8_t chihiros_checksum_xor(const uint8_t *pkt, size_t len_without_checksum)
{
    if (pkt == NULL || len_without_checksum < 2) {
        return 0;
    }

    uint8_t x = 0;
    // XOR of every byte after 0x5a (i.e. starting at index 1)
    for (size_t i = 1; i < len_without_checksum; i++) {
        x ^= pkt[i];
    }
    return x;
}

static void protocol_temp_from_f(float target_f, int *whole_c_out, uint8_t *fine_out)
{
    // Mirror the Python logic in the reverse engineering summary (section 8).
    float target_c = (target_f - 32.0f) * (5.0f / 9.0f);

    int whole_c = (int)floorf(target_c + 1e-6f);
    float block_start_f = (float)whole_c * (9.0f / 5.0f) + 32.0f;
    float delta_f = target_f - block_start_f;

    if (delta_f < -0.0001f) {
        whole_c -= 1;
        block_start_f = (float)whole_c * (9.0f / 5.0f) + 32.0f;
        delta_f = target_f - block_start_f;
    }

    int fine = (int)lroundf(FINE_AT_BLOCK_START + delta_f * FINE_COUNTS_PER_F);

    // If fine overflows the block, carry into next whole-C block and recompute.
    if (fine > 0x63) {
        whole_c += 1;
        block_start_f = (float)whole_c * (9.0f / 5.0f) + 32.0f;
        delta_f = target_f - block_start_f;
        fine = (int)lroundf(FINE_AT_BLOCK_START + delta_f * FINE_COUNTS_PER_F);
    }

    if (fine < 0) fine = 0;
    if (fine > 0xFF) fine = 0xFF;

    *whole_c_out = whole_c;
    *fine_out = (uint8_t)fine;
}

bool chihiros_make_setpoint_packet_f(float target_f, float min_f, float max_f, uint8_t out[11])
{
    if (out == NULL) {
        return false;
    }

    if (!(min_f < max_f)) {
        min_f = 50.0f;
        max_f = 95.0f;
    }

    float clamped_f = clampf(target_f, min_f, max_f);

    int whole_c = 0;
    uint8_t fine = 0;
    protocol_temp_from_f(clamped_f, &whole_c, &fine);

    // Packet: 5a 01 09 00 05 2b 01 WHOLE_C FINE 32 CHECKSUM
    out[0] = 0x5a;
    out[1] = 0x01;
    out[2] = 0x09;
    out[3] = 0x00;
    out[4] = SETPOINT_SEQ;
    out[5] = SETPOINT_CMD;
    out[6] = SETPOINT_CONST_1;
    out[7] = (uint8_t)whole_c;
    out[8] = fine;
    out[9] = SETPOINT_CONST_2;
    out[10] = chihiros_checksum_xor(out, 10);

    return true;
}

static bool decode_temp_sane(float temp_c)
{
    // "Sane aquarium range" — conservative to filter startup/garbage.
    // Typical aquariums: ~18C-32C. We'll allow a wider window.
    return temp_c >= 5.0f && temp_c <= 45.0f;
}

bool chihiros_decode_status_packet(const uint8_t *pkt, size_t len, chihiros_status_t *out)
{
    if (pkt == NULL || out == NULL) {
        return false;
    }

    memset(out, 0, sizeof(*out));
    out->valid = false;

    // Expected format:
    // 5b 03 0e A B WHOLE_C WATTS_H WATTS_L 0b ff TEMP_H TEMP_L 00
    if (len < 13) {
        return false;
    }
    if (pkt[0] != 0x5b || pkt[1] != 0x03 || pkt[2] != 0x0e) {
        return false;
    }

    out->status_a = pkt[3];
    out->status_b = pkt[4];
    out->whole_c_byte = pkt[5];

    uint16_t watts = (uint16_t)((pkt[6] << 8) | pkt[7]);
    out->watts = watts;
    out->heating = watts > 0;

    uint16_t raw_temp = (uint16_t)((pkt[10] << 8) | pkt[11]);
    if (raw_temp == 0xFFFF) {
        return false;
    }

    float temp_c = (float)raw_temp / 10.0f;
    if (!decode_temp_sane(temp_c)) {
        return false;
    }

    out->current_temp_c = temp_c;
    out->current_temp_f = temp_c * (9.0f / 5.0f) + 32.0f;
    out->valid = true;
    return true;
}


#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Nordic UART Service framing used by Chihiros heater commands/notifications.
// Protocol details are sourced from:
// /home/surfacepro/Downloads/chihiros_ble_reverse_engineering_summary.md

typedef struct {
    bool valid;
    uint8_t status_a;
    uint8_t status_b;
    uint8_t whole_c_byte;  // captured but not relied upon

    float current_temp_c;
    float current_temp_f;

    uint16_t watts;
    bool heating;  // derived from watts > 0
} chihiros_status_t;

// Compute XOR checksum: XOR of bytes after 0x5a, excluding checksum byte.
// Expects pkt[0] == 0x5a and len_without_checksum >= 2.
uint8_t chihiros_checksum_xor(const uint8_t *pkt, size_t len_without_checksum);

// Build setpoint command packet (11 bytes) from Fahrenheit.
// - Clamps to [min_f, max_f]
// - Uses reverse-engineered linear mapping inside each whole-C block.
// Returns false only on invalid arguments (e.g. out == NULL).
bool chihiros_make_setpoint_packet_f(float target_f, float min_f, float max_f, uint8_t out[11]);

// Decode a status notification packet.
// Returns true if the packet is recognized and contains a sane temperature (i.e. status->valid == true).
// Returns false if unrecognized or explicitly invalid (e.g. temp == 0xffff or out-of-range).
bool chihiros_decode_status_packet(const uint8_t *pkt, size_t len, chihiros_status_t *out);

// Equivalent tests (deterministic), intended for early bring-up if Unity tests aren't available.
// Returns true iff all acceptance vectors pass.
bool chihiros_protocol_run_selftests(void);

#ifdef __cplusplus
}
#endif


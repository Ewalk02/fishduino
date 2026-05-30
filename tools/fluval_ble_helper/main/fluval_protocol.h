#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    FLUVAL_MODE_UNKNOWN = 0,
    FLUVAL_MODE_MANUAL,
    FLUVAL_MODE_AUTO,
} fluval_mode_t;

typedef struct {
    bool valid;
    fluval_mode_t mode;
    uint8_t pink;
    uint8_t blue;
    uint8_t cold_white;
    uint8_t white;
    uint8_t warm_white;
    uint8_t avg_output;
    int rssi;
    uint32_t last_update_ms;
} fluval_state_t;

typedef enum {
    FLUVAL_ACK_NONE = 0,
    FLUVAL_ACK_MODE_MANUAL,
    FLUVAL_ACK_MODE_AUTO,
    FLUVAL_ACK_SET_CHANNELS,
    FLUVAL_ACK_STATUS,
} fluval_ack_type_t;

#define FLUVAL_STATUS_QUERY_LEN  2
#define FLUVAL_SET_MANUAL_LEN      4
#define FLUVAL_SET_AUTO_LEN        4
#define FLUVAL_SET_CHANNELS_LEN    17
#define FLUVAL_NOTIFY_MAX_LEN      256

uint8_t fluval_clamp_percent(int value);

size_t fluval_build_status_query(uint8_t *out, size_t out_len);
size_t fluval_build_set_manual(uint8_t *out, size_t out_len);
size_t fluval_build_set_auto(uint8_t *out, size_t out_len);
size_t fluval_build_set_channels(uint8_t pink, uint8_t blue, uint8_t cold_white, uint8_t white,
                                 uint8_t warm_white, uint8_t *out, size_t out_len);

bool fluval_parse_status_packet(const uint8_t *data, size_t len, fluval_state_t *out);
fluval_ack_type_t fluval_parse_ack_packet(const uint8_t *data, size_t len, fluval_state_t *status_out);

const char *fluval_mode_to_string(fluval_mode_t mode);
bool fluval_protocol_run_selftests(void);

#ifdef __cplusplus
}
#endif

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    FLUVAL_PROTOCOL_MODE_UNKNOWN = 0,
    FLUVAL_PROTOCOL_MODE_MANUAL,
    FLUVAL_PROTOCOL_MODE_AUTO,
} fluval_protocol_mode_t;

typedef struct {
    bool valid;
    fluval_protocol_mode_t mode;
    uint8_t pink;
    uint8_t blue;
    uint8_t cold_white;
    uint8_t white;
    uint8_t warm_white;
    uint8_t avg_output;
} fluval_protocol_status_t;

#define FLUVAL_PROTOCOL_STATUS_QUERY_LEN 2
#define FLUVAL_PROTOCOL_SET_MANUAL_LEN   4
#define FLUVAL_PROTOCOL_SET_AUTO_LEN     4
#define FLUVAL_PROTOCOL_SET_CHANNELS_LEN 17

uint8_t fluval_protocol_clamp_percent(int value);

size_t fluval_protocol_build_status_query(uint8_t *out, size_t out_len);
size_t fluval_protocol_build_set_manual(uint8_t *out, size_t out_len);
size_t fluval_protocol_build_set_auto(uint8_t *out, size_t out_len);
size_t fluval_protocol_build_set_channels(uint8_t pink, uint8_t blue, uint8_t cold_white, uint8_t white,
                                          uint8_t warm_white, uint8_t *out, size_t out_len);

bool fluval_protocol_parse_status(const uint8_t *data, size_t len, fluval_protocol_status_t *out);
bool fluval_protocol_run_selftests(void);

#ifdef __cplusplus
}
#endif

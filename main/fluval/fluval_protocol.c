#include "fluval_protocol.h"

#include <string.h>

static const uint8_t STATUS_HEADER[] = {0xd2, 0xb0, 0x00, 0x0e, 0x01};
static const uint8_t CHANNEL_MARKER[] = {0x02, 0xf5};

static const uint8_t FIELD_PINK = 0x03;
static const uint8_t FIELD_BLUE = 0x04;
static const uint8_t FIELD_COLD_WHITE = 0x05;
static const uint8_t FIELD_WHITE = 0x06;
static const uint8_t FIELD_WARM_WHITE = 0x07;

static const uint8_t TEST_MANUAL_STATUS[] = {
    0xd2, 0xb0, 0x00, 0x0e, 0x01, 0x00, 0x02, 0xf5, 0x03, 0x18, 0x28, 0x04, 0x14, 0x05, 0x18, 0x3c, 0x06,
    0x18, 0x46, 0x07, 0x18, 0x32,
};

static const uint8_t TEST_AUTO_STATUS[] = {
    0xd2, 0xb0, 0x00, 0x0e, 0x01, 0x01, 0x02, 0xf5, 0x03, 0x18, 0x28, 0x04, 0x14, 0x05, 0x18, 0x3c, 0x06,
    0x18, 0x46, 0x07, 0x18, 0x32,
};

uint8_t fluval_protocol_clamp_percent(int value)
{
    if (value < 0) {
        return 0;
    }
    if (value > 100) {
        return 100;
    }
    return (uint8_t)value;
}

size_t fluval_protocol_build_status_query(uint8_t *out, size_t out_len)
{
    if (out == NULL || out_len < FLUVAL_PROTOCOL_STATUS_QUERY_LEN) {
        return 0;
    }
    out[0] = 0xd0;
    out[1] = 0xff;
    return FLUVAL_PROTOCOL_STATUS_QUERY_LEN;
}

size_t fluval_protocol_build_set_manual(uint8_t *out, size_t out_len)
{
    if (out == NULL || out_len < FLUVAL_PROTOCOL_SET_MANUAL_LEN) {
        return 0;
    }
    out[0] = 0xd1;
    out[1] = 0xa1;
    out[2] = 0x01;
    out[3] = 0x00;
    return FLUVAL_PROTOCOL_SET_MANUAL_LEN;
}

size_t fluval_protocol_build_set_auto(uint8_t *out, size_t out_len)
{
    if (out == NULL || out_len < FLUVAL_PROTOCOL_SET_AUTO_LEN) {
        return 0;
    }
    out[0] = 0xd1;
    out[1] = 0xa1;
    out[2] = 0x01;
    out[3] = 0x01;
    return FLUVAL_PROTOCOL_SET_AUTO_LEN;
}

size_t fluval_protocol_build_set_channels(uint8_t pink, uint8_t blue, uint8_t cold_white, uint8_t white,
                                          uint8_t warm_white, uint8_t *out, size_t out_len)
{
    if (out == NULL || out_len < FLUVAL_PROTOCOL_SET_CHANNELS_LEN) {
        return 0;
    }

    const struct {
        uint8_t field;
        uint8_t percent;
    } channels[] = {
        {FIELD_PINK, fluval_protocol_clamp_percent(pink)},
        {FIELD_BLUE, fluval_protocol_clamp_percent(blue)},
        {FIELD_COLD_WHITE, fluval_protocol_clamp_percent(cold_white)},
        {FIELD_WHITE, fluval_protocol_clamp_percent(white)},
        {FIELD_WARM_WHITE, fluval_protocol_clamp_percent(warm_white)},
    };

    size_t idx = 0;
    out[idx++] = 0xd1;
    out[idx++] = 0xa6;
    for (size_t i = 0; i < sizeof(channels) / sizeof(channels[0]); i++) {
        out[idx++] = channels[i].field;
        out[idx++] = 0x18;
        out[idx++] = channels[i].percent;
    }
    out[idx++] = 0x0e;
    out[idx++] = 0x00;
    return idx;
}

static const uint8_t *find_marker(const uint8_t *data, size_t len)
{
    if (data == NULL || len < sizeof(CHANNEL_MARKER)) {
        return NULL;
    }

    for (size_t i = 0; i + sizeof(CHANNEL_MARKER) <= len; i++) {
        if (memcmp(data + i, CHANNEL_MARKER, sizeof(CHANNEL_MARKER)) == 0) {
            return data + i + sizeof(CHANNEL_MARKER);
        }
    }
    return NULL;
}

static bool parse_channel_field(const uint8_t *data, size_t len, size_t *idx, uint8_t expected_field,
                                uint8_t *percent_out)
{
    if (data == NULL || idx == NULL || percent_out == NULL || *idx >= len) {
        return false;
    }
    if (data[*idx] != expected_field) {
        return false;
    }
    (*idx)++;
    if (*idx >= len) {
        return false;
    }

    if (data[*idx] == 0x18) {
        (*idx)++;
        if (*idx >= len) {
            return false;
        }
        *percent_out = fluval_protocol_clamp_percent(data[*idx]);
        (*idx)++;
        return true;
    }

    *percent_out = fluval_protocol_clamp_percent(data[*idx]);
    (*idx)++;
    return true;
}

static uint8_t compute_avg(uint8_t pink, uint8_t blue, uint8_t cold_white, uint8_t white, uint8_t warm_white)
{
    unsigned sum = (unsigned)pink + blue + cold_white + white + warm_white;
    return (uint8_t)((sum + 2U) / 5U);
}

bool fluval_protocol_parse_status(const uint8_t *data, size_t len, fluval_protocol_status_t *out)
{
    if (out == NULL) {
        return false;
    }

    memset(out, 0, sizeof(*out));
    out->mode = FLUVAL_PROTOCOL_MODE_UNKNOWN;

    if (data == NULL || len < 8) {
        return false;
    }

    if (memcmp(data, STATUS_HEADER, sizeof(STATUS_HEADER)) != 0) {
        return false;
    }

    switch (data[5]) {
    case 0x00:
        out->mode = FLUVAL_PROTOCOL_MODE_MANUAL;
        break;
    case 0x01:
        out->mode = FLUVAL_PROTOCOL_MODE_AUTO;
        break;
    default:
        out->mode = FLUVAL_PROTOCOL_MODE_UNKNOWN;
        break;
    }

    const uint8_t *channel_start = find_marker(data, len);
    if (channel_start == NULL) {
        return false;
    }

    size_t idx = (size_t)(channel_start - data);
    if (!parse_channel_field(data, len, &idx, FIELD_PINK, &out->pink)) {
        return false;
    }
    if (!parse_channel_field(data, len, &idx, FIELD_BLUE, &out->blue)) {
        return false;
    }
    if (!parse_channel_field(data, len, &idx, FIELD_COLD_WHITE, &out->cold_white)) {
        return false;
    }
    if (!parse_channel_field(data, len, &idx, FIELD_WHITE, &out->white)) {
        return false;
    }
    if (!parse_channel_field(data, len, &idx, FIELD_WARM_WHITE, &out->warm_white)) {
        return false;
    }

    out->avg_output = compute_avg(out->pink, out->blue, out->cold_white, out->white, out->warm_white);
    out->valid = true;
    return true;
}

static const uint8_t ACK_MANUAL[] = {0xd2, 0xa1, 0x01, 0x00};
static const uint8_t ACK_AUTO[] = {0xd2, 0xa1, 0x01, 0x01};

const char *fluval_protocol_mode_token(fluval_protocol_mode_t mode)
{
    switch (mode) {
    case FLUVAL_PROTOCOL_MODE_MANUAL:
        return "MANUAL";
    case FLUVAL_PROTOCOL_MODE_AUTO:
        return "AUTO";
    default:
        return "UNKNOWN";
    }
}

fluval_protocol_ack_t fluval_protocol_parse_ack(const uint8_t *data, size_t len, fluval_protocol_status_t *status_out)
{
    if (data == NULL || len < 4) {
        return FLUVAL_PROTOCOL_ACK_NONE;
    }

    if (len >= sizeof(ACK_MANUAL) && memcmp(data, ACK_MANUAL, sizeof(ACK_MANUAL)) == 0) {
        return FLUVAL_PROTOCOL_ACK_MODE_MANUAL;
    }
    if (len >= sizeof(ACK_AUTO) && memcmp(data, ACK_AUTO, sizeof(ACK_AUTO)) == 0) {
        return FLUVAL_PROTOCOL_ACK_MODE_AUTO;
    }
    if (len >= 2 && data[0] == 0xd2 && data[1] == 0xa6) {
        return FLUVAL_PROTOCOL_ACK_SET_CHANNELS;
    }
    if (len >= sizeof(STATUS_HEADER) && memcmp(data, STATUS_HEADER, sizeof(STATUS_HEADER)) == 0) {
        if (status_out != NULL) {
            fluval_protocol_parse_status(data, len, status_out);
        }
        return FLUVAL_PROTOCOL_ACK_STATUS;
    }

    return FLUVAL_PROTOCOL_ACK_NONE;
}

static bool selftest_parse_acks(void)
{
    static const uint8_t ack_manual[] = {0xd2, 0xa1, 0x01, 0x00};
    fluval_protocol_status_t st = {0};

    if (fluval_protocol_parse_ack(ack_manual, sizeof(ack_manual), NULL) != FLUVAL_PROTOCOL_ACK_MODE_MANUAL) {
        return false;
    }
    if (fluval_protocol_parse_ack(TEST_MANUAL_STATUS, sizeof(TEST_MANUAL_STATUS), &st) !=
        FLUVAL_PROTOCOL_ACK_STATUS) {
        return false;
    }
    if (!st.valid || st.mode != FLUVAL_PROTOCOL_MODE_MANUAL) {
        return false;
    }
    return true;
}

static bool selftest_build_commands(void)
{
    uint8_t buf[32];

    if (fluval_protocol_build_status_query(buf, sizeof(buf)) != FLUVAL_PROTOCOL_STATUS_QUERY_LEN) {
        return false;
    }
    if (buf[0] != 0xd0 || buf[1] != 0xff) {
        return false;
    }

    if (fluval_protocol_build_set_manual(buf, sizeof(buf)) != FLUVAL_PROTOCOL_SET_MANUAL_LEN) {
        return false;
    }
    if (buf[0] != 0xd1 || buf[1] != 0xa1 || buf[2] != 0x01 || buf[3] != 0x00) {
        return false;
    }

    if (fluval_protocol_build_set_auto(buf, sizeof(buf)) != FLUVAL_PROTOCOL_SET_AUTO_LEN) {
        return false;
    }
    if (buf[3] != 0x01) {
        return false;
    }

    static const uint8_t expected_mix[] = {0xd1, 0xa6, 0x03, 0x18, 0x28, 0x04, 0x18, 0x14, 0x05, 0x18, 0x3c,
                                           0x06, 0x18, 0x46, 0x07, 0x18, 0x32, 0x0e, 0x00};
    size_t mix_len =
        fluval_protocol_build_set_channels(40, 20, 60, 70, 50, buf, sizeof(buf));
    if (mix_len != sizeof(expected_mix) || memcmp(buf, expected_mix, sizeof(expected_mix)) != 0) {
        return false;
    }

    static const uint8_t expected_all_25[] = {0xd1, 0xa6, 0x03, 0x18, 0x19, 0x04, 0x18, 0x19, 0x05, 0x18, 0x19,
                                                0x06, 0x18, 0x19, 0x07, 0x18, 0x19, 0x0e, 0x00};
    size_t all_25_len = fluval_protocol_build_set_channels(25, 25, 25, 25, 25, buf, sizeof(buf));
    if (all_25_len != sizeof(expected_all_25) ||
        memcmp(buf, expected_all_25, sizeof(expected_all_25)) != 0) {
        return false;
    }

    return true;
}

static bool selftest_parse_manual_auto(void)
{
    fluval_protocol_status_t st = {0};

    if (!fluval_protocol_parse_status(TEST_MANUAL_STATUS, sizeof(TEST_MANUAL_STATUS), &st)) {
        return false;
    }
    if (!st.valid || st.mode != FLUVAL_PROTOCOL_MODE_MANUAL || st.pink != 40 || st.blue != 20 ||
        st.cold_white != 60 || st.white != 70 || st.warm_white != 50 || st.avg_output != 48) {
        return false;
    }

    memset(&st, 0, sizeof(st));
    if (!fluval_protocol_parse_status(TEST_AUTO_STATUS, sizeof(TEST_AUTO_STATUS), &st)) {
        return false;
    }
    if (!st.valid || st.mode != FLUVAL_PROTOCOL_MODE_AUTO || st.avg_output != 48) {
        return false;
    }

    return true;
}

static bool selftest_parse_failures(void)
{
    fluval_protocol_status_t st = {0};
    const uint8_t short_pkt[] = {0xd2, 0xb0};
    const uint8_t no_marker[] = {0xd2, 0xb0, 0x00, 0x0e, 0x01, 0x00, 0x02, 0x00};

    if (fluval_protocol_parse_status(NULL, 0, &st)) {
        return false;
    }
    if (fluval_protocol_parse_status(short_pkt, sizeof(short_pkt), &st)) {
        return false;
    }
    if (fluval_protocol_parse_status(no_marker, sizeof(no_marker), &st)) {
        return false;
    }

    return true;
}

bool fluval_protocol_run_selftests(void)
{
    return selftest_build_commands() && selftest_parse_manual_auto() && selftest_parse_failures() &&
           selftest_parse_acks();
}

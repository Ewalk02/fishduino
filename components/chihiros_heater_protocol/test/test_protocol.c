#include "unity.h"

#include "chihiros_heater_protocol.h"

static void assert_bytes_eq(const uint8_t *a, const uint8_t *b, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        TEST_ASSERT_EQUAL_HEX8_MESSAGE(b[i], a[i], "byte mismatch");
    }
}

TEST_CASE("chihiros setpoint packet vectors", "[chihiros][protocol]")
{
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
        TEST_ASSERT_TRUE(chihiros_make_setpoint_packet_f(cases[i].f, 50.0f, 95.0f, pkt));
        assert_bytes_eq(pkt, cases[i].expected, sizeof(pkt));
    }
}

TEST_CASE("chihiros checksum vectors", "[chihiros][protocol]")
{
    {
        uint8_t pkt10[] = {0x5a, 0x01, 0x09, 0x00, 0x05, 0x2b, 0x01, 0x19, 0x06, 0x32};
        TEST_ASSERT_EQUAL_HEX8(0x0a, chihiros_checksum_xor(pkt10, sizeof(pkt10)));
    }
    {
        uint8_t pkt10[] = {0x5a, 0x01, 0x09, 0x00, 0x05, 0x2b, 0x01, 0x19, 0x55, 0x32};
        TEST_ASSERT_EQUAL_HEX8(0x59, chihiros_checksum_xor(pkt10, sizeof(pkt10)));
    }
}

TEST_CASE("chihiros status decode vectors", "[chihiros][protocol]")
{
    {
        uint8_t status[] = {0x5b, 0x03, 0x0e, 0x00, 0x25, 0x25, 0x00, 0x00, 0x0b, 0xff, 0x00, 0xfe, 0x00};
        chihiros_status_t st = {0};
        TEST_ASSERT_TRUE(chihiros_decode_status_packet(status, sizeof(status), &st));
        TEST_ASSERT_TRUE(st.valid);
        TEST_ASSERT_EQUAL_UINT16(0, st.watts);
        TEST_ASSERT_FALSE(st.heating);
        TEST_ASSERT_FLOAT_WITHIN(0.01f, 25.4f, st.current_temp_c);
        TEST_ASSERT_FLOAT_WITHIN(0.05f, 77.72f, st.current_temp_f);
        TEST_ASSERT_EQUAL_HEX8(0x00, st.status_a);
        TEST_ASSERT_EQUAL_HEX8(0x25, st.status_b);
    }

    {
        uint8_t status[] = {0x5b, 0x03, 0x0e, 0x00, 0x25, 0x25, 0x02, 0x72, 0x0b, 0xff, 0x00, 0xfe, 0x00};
        chihiros_status_t st = {0};
        TEST_ASSERT_TRUE(chihiros_decode_status_packet(status, sizeof(status), &st));
        TEST_ASSERT_TRUE(st.valid);
        TEST_ASSERT_EQUAL_UINT16(626, st.watts);
        TEST_ASSERT_TRUE(st.heating);
        TEST_ASSERT_FLOAT_WITHIN(0.01f, 25.4f, st.current_temp_c);
        TEST_ASSERT_FLOAT_WITHIN(0.05f, 77.72f, st.current_temp_f);
        TEST_ASSERT_EQUAL_HEX8(0x00, st.status_a);
        TEST_ASSERT_EQUAL_HEX8(0x25, st.status_b);
    }

    {
        // Invalid/startup packet with TEMP=ffff should be rejected.
        uint8_t status[] = {0x5b, 0x03, 0x0e, 0x00, 0x01, 0x0a, 0x00, 0x00, 0x0f, 0xff, 0xff, 0xff, 0x22, 0x05, 0x00, 0x22};
        chihiros_status_t st = {0};
        TEST_ASSERT_FALSE(chihiros_decode_status_packet(status, sizeof(status), &st));
        TEST_ASSERT_FALSE(st.valid);
    }
}


#include <unity.h>
#include <cstring>
#include <initializer_list>
#include "SRXL2.h"

// Complete wire fixtures, CRCs independently calculated with Python crc_hqx.
static const uint8_t hello[] = {0xA6,0x21,0x0E,0x40,0x21,0x0A,0,0,0x11,0x22,0x33,0x44,0xC9,0xE7};
static const uint8_t esc[] = {0xA6,0x80,0x16,0x21,0x20,0,0x30,0x39,0x04,0xD2,0x01,0x5E,0x03,0xE8,0,0xFA,0x0F,0x78,0x64,0x64,0x74,0x74};
static const uint8_t unavailable[] = {0xA6,0x80,0x16,0x21,0x20,0,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0x61,0x9A};
static const uint8_t battery[] = {0xA6,0x80,0x16,0x21,0x42,0,0,0xF6,0xB8,0x0B,0,0,0x39,0x30,0x01,0x0E,0xD0,0x0E,0,0,0x97,0x9C};
static const uint8_t cells[] = {0xA6,0x80,0x16,0x21,0x42,0,0x10,0x19,0x01,0x10,0x02,0x10,0x03,0x10,0xFF,0xFF,0,0,0xFF,0xFF,0x28,0xD3};
static const uint8_t identity[] = {0xA6,0x80,0x16,0x21,0x42,0,0x80,0x01,0x03,0x01,0x01,0,0x11,0x22,0x33,0x44,0x55,0x66,0x77,0x88,0x4D,0x81};

void setUp() {}
void tearDown() {}

static void feed(SRXL2::Link &link, const uint8_t *bytes, size_t size, uint32_t now)
{
    for (size_t i = 0; i < size; ++i) link.receive(bytes[i], now);
}

static void connect(SRXL2::Link &link, uint32_t offset = 0)
{
    link.reset(0x12345678, offset);
    SRXL2::Packet packet{};
    TEST_ASSERT_TRUE(link.nextPacket(offset + 50000, packet));
    link.transmitted(offset + 51200);
    feed(link, hello, sizeof(hello), offset + 51400);
    TEST_ASSERT_TRUE(link.nextPacket(offset + 51600, packet));
    link.transmitted(offset + 52800);
    TEST_ASSERT_TRUE(link.connected());
}

static uint16_t channel(const SRXL2::Packet &packet)
{
    return packet.bytes[12] | (uint16_t(packet.bytes[13]) << 8);
}

void test_crc_and_nominal_throttle()
{
    uint8_t text[] = "123456789";
    TEST_ASSERT_EQUAL_HEX16(0x31C3, SRXL2::crc16(text, 9));
    TEST_ASSERT_EQUAL_HEX16(0x2AA0, SRXL2::encodeThrottle(172));
    TEST_ASSERT_EQUAL_HEX16(0x8000, SRXL2::encodeThrottle(992));
    TEST_ASSERT_EQUAL_HEX16(0xD554, SRXL2::encodeThrottle(1811));
    TEST_ASSERT_EQUAL_HEX16(0x2AA0, SRXL2::encodeThrottle(0));
    TEST_ASSERT_EQUAL_HEX16(0xD554, SRXL2::encodeThrottle(2047));
    for (uint16_t value = 0; value < 2048; ++value)
        TEST_ASSERT_EQUAL(0, SRXL2::encodeThrottle(value) & 3);
}

void test_handshake_vectors_and_tx_ownership()
{
    SRXL2::Link link;
    link.reset(0x12345678, 0);
    SRXL2::Packet packet{};
    const uint8_t request[] = {0xA6,0x21,0x0E,0x21,0x40,0x0A,0,3,0x78,0x56,0x34,0x12,0xF3,0x91};
    const uint8_t broadcast[] = {0xA6,0x21,0x0E,0x21,0xFF,0x0A,0,3,0x78,0x56,0x34,0x12,0x91,0x0E};
    TEST_ASSERT_FALSE(link.nextPacket(49999, packet));
    TEST_ASSERT_TRUE(link.nextPacket(50000, packet));
    TEST_ASSERT_EQUAL(14, packet.length);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(request, packet.bytes, 14);
    TEST_ASSERT_FALSE(link.nextPacket(60000, packet));
    // A local echo must not complete discovery.
    feed(link, request, sizeof(request), 50100);
    link.transmitted(51200);
    TEST_ASSERT_FALSE(link.nextPacket(51400, packet));
    feed(link, hello, sizeof(hello), 51400);
    TEST_ASSERT_FALSE(link.connected());
    TEST_ASSERT_FALSE(link.nextPacket(51573, packet));
    TEST_ASSERT_TRUE(link.nextPacket(51600, packet));
    TEST_ASSERT_EQUAL_UINT8_ARRAY(broadcast, packet.bytes, 14);
    link.transmitted(52800);
    TEST_ASSERT_TRUE(link.connected());
}

void test_startup_neutral_release_and_failsafe_vector()
{
    SRXL2::Link link;
    connect(link);
    SRXL2::Packet packet{};
    const uint8_t safe[] = {0xA6,0xCD,0x10,1,0x40,0,0,0,1,0,0,0,0,0x80,0x2C,0x19};
    link.setControlPermission(true);
    link.setThrottle(1811, 53000);
    TEST_ASSERT_TRUE(link.nextPacket(53000, packet));
    TEST_ASSERT_EQUAL_UINT8_ARRAY(safe, packet.bytes, 16);
    link.transmitted(54400);
    feed(link, esc, sizeof(esc), 55000);
    link.setThrottle(992, 56000);
    TEST_ASSERT_TRUE(link.nextPacket(63000, packet));
    TEST_ASSERT_EQUAL(0, packet.bytes[3]);
    TEST_ASSERT_EQUAL_HEX16(0x8000, channel(packet));
    link.transmitted(64400);
    link.setThrottle(1811, 65000);
    TEST_ASSERT_TRUE(link.nextPacket(73000, packet));
    TEST_ASSERT_EQUAL_HEX16(0xD554, channel(packet));
    link.transmitted(74400);
    link.setControlPermission(false);
    TEST_ASSERT_TRUE(link.nextPacket(83000, packet));
    TEST_ASSERT_EQUAL(1, packet.bytes[3]);
    TEST_ASSERT_EQUAL_HEX16(0x8000, channel(packet));
}

void test_neutral_band_boundaries_and_permission_recovery()
{
    for (uint16_t neutral : {uint16_t(976), uint16_t(1008)})
    {
        SRXL2::Link link;
        connect(link);
        SRXL2::Packet packet{};
        link.setControlPermission(true);
        link.setThrottle(neutral, 53000);
        TEST_ASSERT_TRUE(link.nextPacket(53000, packet));
        TEST_ASSERT_EQUAL(0, packet.bytes[3]);
        TEST_ASSERT_EQUAL_HEX16(0x8000, channel(packet));
        link.transmitted(54400);
        feed(link, esc, sizeof(esc), 55000);
        link.setControlPermission(false);
        link.setControlPermission(true);
        link.setThrottle(1811, 63000);
        TEST_ASSERT_TRUE(link.nextPacket(63000, packet));
        TEST_ASSERT_EQUAL(1, packet.bytes[3]);
        TEST_ASSERT_EQUAL_HEX16(0x8000, channel(packet));
    }
}

void test_stale_control_needs_new_neutral()
{
    SRXL2::Link link;
    connect(link);
    SRXL2::Packet packet{};
    link.setControlPermission(true);
    link.setThrottle(992, 53000);
    TEST_ASSERT_TRUE(link.nextPacket(53000, packet));
    link.transmitted(54400);
    feed(link, esc, sizeof(esc), 55000);
    link.setThrottle(1811, 56000);
    TEST_ASSERT_TRUE(link.nextPacket(63000, packet));
    link.transmitted(64400);
    TEST_ASSERT_TRUE(link.nextPacket(156000, packet));
    TEST_ASSERT_EQUAL(1, packet.bytes[3]);
    TEST_ASSERT_EQUAL_HEX16(0x8000, channel(packet));
    link.transmitted(157400);
    feed(link, esc, sizeof(esc), 160000);
    link.setThrottle(1811, 166000);
    TEST_ASSERT_TRUE(link.nextPacket(166000, packet));
    TEST_ASSERT_EQUAL_HEX16(0x8000, channel(packet));
}

void test_response_window_and_control_cadence()
{
    SRXL2::Link link;
    connect(link);
    SRXL2::Packet packet{};
    TEST_ASSERT_TRUE(link.nextPacket(53000, packet));
    link.transmitted(54400);
    TEST_ASSERT_FALSE(link.nextPacket(55000, packet));
    TEST_ASSERT_FALSE(link.nextPacket(62399, packet));
    TEST_ASSERT_FALSE(link.nextPacket(62999, packet));
    TEST_ASSERT_TRUE(link.nextPacket(63000, packet));
    TEST_ASSERT_EQUAL(0, packet.bytes[4]);
}

void test_esc_scaling_and_invalid_replacement()
{
    SRXL2::Link link;
    connect(link);
    feed(link, esc, sizeof(esc), 60000);
    auto values = link.telemetry(60000);
    TEST_ASSERT_EQUAL(123450, values.rpm.value);
    TEST_ASSERT_EQUAL(12340, values.voltage.value);
    TEST_ASSERT_EQUAL(10000, values.current.value);
    TEST_ASSERT_EQUAL(350, values.temperatureFet.value);
    TEST_ASSERT_EQUAL(250, values.temperatureBec.value);
    TEST_ASSERT_EQUAL(1500, values.currentBec.value);
    TEST_ASSERT_EQUAL(6000, values.voltageBec.value);
    TEST_ASSERT_TRUE(values.voltage.valid);
    TEST_ASSERT_FALSE(values.consumption.valid);
    feed(link, unavailable, sizeof(unavailable), 70000);
    values = link.telemetry(70000);
    TEST_ASSERT_FALSE(values.rpm.valid);
    TEST_ASSERT_FALSE(values.current.valid);
    TEST_ASSERT_FALSE(values.voltageBec.valid);
}

void test_smart_battery_endian_and_cells()
{
    SRXL2::Link link;
    connect(link);
    feed(link, battery, sizeof(battery), 60000);
    feed(link, cells, sizeof(cells), 61000);
    feed(link, identity, sizeof(identity), 62000);
    auto values = link.telemetry(62000);
    TEST_ASSERT_EQUAL(3000, values.batteryCurrent.value);
    TEST_ASSERT_EQUAL(1234, values.consumption.value);
    TEST_ASSERT_EQUAL(250, values.batteryTemperature.value);
    TEST_ASSERT_EQUAL(3, values.cellCount);
    TEST_ASSERT_EQUAL(4097, values.cells[0].value);
    TEST_ASSERT_EQUAL(4098, values.cells[1].value);
    TEST_ASSERT_EQUAL(4099, values.cells[2].value);
    TEST_ASSERT_FALSE(values.cells[3].valid);
    values = link.telemetry(2060000);
    TEST_ASSERT_FALSE(values.batteryCurrent.valid);
    TEST_ASSERT_TRUE(values.cells[0].valid);
    TEST_ASSERT_FALSE(link.telemetry(5061000).cells[0].valid);
}

void test_parser_recovers_from_noise_bad_crc_and_partial_timeout()
{
    SRXL2::Link link;
    connect(link);
    const uint8_t noise[] = {0, 0xA6, 0x80, 0xFF, 0xA6, 0x80, 4, 0};
    feed(link, noise, sizeof(noise), 60000);
    uint8_t broken[sizeof(esc)];
    std::memcpy(broken, esc, sizeof(esc));
    broken[21] ^= 1;
    feed(link, broken, sizeof(broken), 61000);
    TEST_ASSERT_FALSE(link.telemetry(61000).voltage.valid);
    feed(link, esc, 5, 62000);
    feed(link, esc, sizeof(esc), 72000);
    TEST_ASSERT_TRUE(link.telemetry(72000).voltage.valid);
    feed(link, battery, 9, 73000);
    feed(link, battery + 9, sizeof(battery) - 9, 73100);
    feed(link, cells, sizeof(cells), 73200);
    TEST_ASSERT_EQUAL(3000, link.telemetry(73200).batteryCurrent.value);
    TEST_ASSERT_EQUAL(4097, link.telemetry(73200).cells[0].value);
}

void test_wrong_master_and_disconnect_clear_data_and_motion()
{
    SRXL2::Link link;
    connect(link);
    uint8_t other[sizeof(esc)];
    std::memcpy(other, esc, sizeof(esc));
    other[3] = 0x22;
    const uint16_t crc = SRXL2::crc16(other, sizeof(other) - 2);
    other[20] = crc >> 8; other[21] = crc;
    feed(link, other, sizeof(other), 60000);
    TEST_ASSERT_FALSE(link.telemetry(60000).voltage.valid);
    feed(link, esc, sizeof(esc), 61000);
    link.setControlPermission(true);
    link.setThrottle(992, 62000);
    link.setThrottle(1811, 63000);
    SRXL2::Packet packet{};
    TEST_ASSERT_TRUE(link.nextPacket(2061000, packet));
    TEST_ASSERT_FALSE(link.connected());
    TEST_ASSERT_FALSE(link.telemetry(2061000).voltage.valid);
    TEST_ASSERT_EQUAL(0x21, packet.bytes[1]);
    link.transmitted(2062200);
    feed(link, hello, sizeof(hello), 2062400);
    TEST_ASSERT_TRUE(link.nextPacket(2062600, packet));
    link.transmitted(2063800);
    link.setThrottle(1811, 2064000);
    TEST_ASSERT_TRUE(link.nextPacket(2064000, packet));
    TEST_ASSERT_EQUAL_HEX16(0x8000, channel(packet));
}

void test_unavailable_fields_are_still_valid_bus_replies()
{
    SRXL2::Link link;
    connect(link);
    feed(link, unavailable, sizeof(unavailable), 2000000);
    SRXL2::Packet packet{};
    TEST_ASSERT_TRUE(link.nextPacket(2100000, packet));
    TEST_ASSERT_TRUE(link.connected());
    TEST_ASSERT_EQUAL(0xCD, packet.bytes[1]);
}

void test_timer_wrap()
{
    SRXL2::Link link;
    const uint32_t start = 0xFFFF0000u;
    connect(link, start);
    link.setControlPermission(true);
    link.setThrottle(992, start + 53000);
    SRXL2::Packet packet{};
    TEST_ASSERT_TRUE(link.nextPacket(start + 53000, packet));
    link.transmitted(start + 54400);
    feed(link, esc, sizeof(esc), start + 55000);
    link.setThrottle(1811, start + 65000);
    TEST_ASSERT_TRUE(link.nextPacket(start + 73000, packet));
    TEST_ASSERT_EQUAL_HEX16(0xD554, channel(packet));
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_crc_and_nominal_throttle);
    RUN_TEST(test_handshake_vectors_and_tx_ownership);
    RUN_TEST(test_startup_neutral_release_and_failsafe_vector);
    RUN_TEST(test_neutral_band_boundaries_and_permission_recovery);
    RUN_TEST(test_stale_control_needs_new_neutral);
    RUN_TEST(test_response_window_and_control_cadence);
    RUN_TEST(test_esc_scaling_and_invalid_replacement);
    RUN_TEST(test_smart_battery_endian_and_cells);
    RUN_TEST(test_parser_recovers_from_noise_bad_crc_and_partial_timeout);
    RUN_TEST(test_wrong_master_and_disconnect_clear_data_and_motion);
    RUN_TEST(test_unavailable_fields_are_still_valid_bus_replies);
    RUN_TEST(test_timer_wrap);
    return UNITY_END();
}

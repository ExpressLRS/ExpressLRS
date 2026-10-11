#include <unity.h>
#include <cstring>
#include <initializer_list>
#include "SRXL2.h"

// Complete wire fixtures, CRCs independently calculated with Python crc_hqx.
static const uint8_t hello[] = {0xA6,0x21,0x0E,0x40,0x21,0x0A,0,0,0x11,0x22,0x33,0x44,0xC9,0xE7};
static const uint8_t hello400[] = {0xA6,0x21,0x0E,0x40,0x21,0x0A,1,0,0x11,0x22,0x33,0x44,0x8C,0x47};
static const uint8_t announce400[] = {0xA6,0x21,0x0E,0x40,0,0x0A,1,0,0,0,0,1,0x7D,0xEE};
static const uint8_t broadcast400[] = {0xA6,0x21,0x0E,0x21,0xFF,0x0A,1,3,0x78,0x56,0x34,0x12,0xD4,0xAE};
static const uint8_t esc[] = {0xA6,0x80,0x16,0x21,0x20,0,0x30,0x39,0x04,0xD2,0x01,0x5E,0x03,0xE8,0,0xFA,0x0F,0x78,0x64,0x64,0x74,0x74};
static const uint8_t unavailable[] = {0xA6,0x80,0x16,0x21,0x20,0,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0x61,0x9A};
static const uint8_t battery[] = {0xA6,0x80,0x16,0x21,0x42,0,0,0xF6,0xB8,0x0B,0,0,0x39,0x30,0x01,0x0E,0xD0,0x0E,0,0,0x97,0x9C};
static const uint8_t cells[] = {0xA6,0x80,0x16,0x21,0x42,0,0x10,0x19,0x01,0x10,0x02,0x10,0x03,0x10,0xFF,0xFF,0,0,0xFF,0xFF,0x28,0xD3};
static const uint8_t identity[] = {0xA6,0x80,0x16,0x21,0x42,0,0x80,0x01,0x03,0x01,0x01,0,0x11,0x22,0x33,0x44,0x55,0x66,0x77,0x88,0x4D,0x81};

void run_adapter_tests();

static void feed(SRXL2::Link &link, const uint8_t *bytes, size_t size, uint32_t now)
{
    for (size_t i = 0; i < size; ++i) link.receive(bytes[i], now);
}

static void connect(SRXL2::Link &link, uint32_t offset = 0)
{
    link.reset(0x12345678, offset);
    SRXL2::Packet packet{};
    // An already-powered ESC can announce during the receiver's startup listen.
    feed(link, hello, sizeof(hello), offset + 31400);
    TEST_ASSERT_TRUE(link.nextPacket(offset + 31600, packet));
    link.transmitted(offset + 32800);
    TEST_ASSERT_TRUE(link.connected());
}

static uint16_t channel(const SRXL2::Packet &packet)
{
    return packet.bytes[12] | (uint16_t(packet.bytes[13]) << 8);
}

static void updateCrc(uint8_t *bytes, uint8_t size)
{
    const uint16_t crc = SRXL2::crc16(bytes, size - 2);
    bytes[size - 2] = crc >> 8;
    bytes[size - 1] = crc;
}

static void connectFast(SRXL2::Link &link, uint32_t offset = 0)
{
    link.reset(0x12345678, offset, false, true);
    feed(link, hello400, sizeof(hello400), offset + 31400);
    SRXL2::Packet packet{};
    TEST_ASSERT_TRUE(link.nextPacket(offset + 31600, packet));
    link.transmitted(offset + 32800);
    link.baudApplied(offset + 32800);
    TEST_ASSERT_TRUE(link.connected());
}

void test_fast_link_polls_safely_without_rf()
{
    SRXL2::Link link;
    connectFast(link);
    SRXL2::Packet packet{};
    for (unsigned i = 0; i < 6; ++i)
    {
        const uint32_t now = 52800 + i * 200000;
        link.setControlPermission(i % 2 == 1);
        TEST_ASSERT_TRUE(link.nextPacket(now, packet));
        TEST_ASSERT_EQUAL_HEX8(0x01, packet.bytes[3]);
        TEST_ASSERT_EQUAL_HEX16(0x8000, channel(packet));
        TEST_ASSERT_EQUAL_HEX8(0x40, packet.bytes[4]);
        link.transmitted(now + 400);
        TEST_ASSERT_FALSE(link.nextPacket(now + 10000, packet));
        TEST_ASSERT_TRUE(link.nextPacket(now + 20400, packet));
        TEST_ASSERT_EQUAL(0, packet.bytes[4]); // Ordinary failsafe frames do not shorten the 200 ms poll interval.
        TEST_ASSERT_EQUAL_HEX16(0x8000, channel(packet));
        link.transmitted(now + 20800);
    }
    link.setControlPermission(true);
    link.poll(2032800); // Permission changes and polls do not extend fast health.
    TEST_ASSERT_FALSE(link.connected());
}

void test_fast_timeout_returns_to_default_after_master_silence()
{
    for (uint32_t offset : {0u, 0xFFFF0000u})
    {
        SRXL2::Link link;
        connectFast(link, offset);
        SRXL2::Packet packet{};
        TEST_ASSERT_TRUE(link.nextPacket(offset + 2032700, packet));
        TEST_ASSERT_FALSE(link.nextPacket(offset + 2032800, packet));
        TEST_ASSERT_FALSE(link.connected());
        TEST_ASSERT_EQUAL_UINT32(400000, link.baudRate());
        link.poll(offset + 2082800); // TX still owns the last stop bit.
        TEST_ASSERT_EQUAL_UINT32(400000, link.baudRate());
        const uint32_t stop = offset + 2083100;
        link.transmitted(stop);
        feed(link, hello400, sizeof(hello400), stop + 1000);
        link.receive(0xA6, stop + 49999); // Even busy partial RX cannot prevent fallback housekeeping.
        link.poll(stop + 49999);
        TEST_ASSERT_EQUAL_UINT32(400000, link.baudRate());
        link.receive(0x80, stop + 50000);
        link.poll(stop + 50000);
        TEST_ASSERT_EQUAL_UINT32(115200, link.baudRate());
        TEST_ASSERT_FALSE(link.nextPacket(stop + 51000, packet));
        link.baudApplied(stop + 51000);
        TEST_ASSERT_FALSE(link.nextPacket(stop + 51173, packet));
        TEST_ASSERT_TRUE(link.nextPacket(stop + 51174, packet));
        TEST_ASSERT_EQUAL_HEX8(0x21, packet.bytes[1]);
        TEST_ASSERT_EQUAL(0, packet.bytes[6]);
        link.transmitted(stop + 52390);
        feed(link, hello400, sizeof(hello400), stop + 52590);
        TEST_ASSERT_TRUE(link.nextPacket(stop + 52764, packet));
        TEST_ASSERT_EQUAL_HEX8(0xFF, packet.bytes[4]);
        TEST_ASSERT_EQUAL(0, packet.bytes[6]);
    }
}

void test_fast_health_accepts_unavailable_replies_but_ignores_echo_and_noise()
{
    for (uint32_t offset : {0u, 0xFFFF0000u})
    {
        SRXL2::Link healthy;
        connectFast(healthy, offset);
        feed(healthy, unavailable, sizeof(unavailable), offset + 1900000);
        healthy.poll(offset + 2032800);
        TEST_ASSERT_TRUE(healthy.connected());
        TEST_ASSERT_EQUAL_UINT32(400000, healthy.baudRate());
        healthy.poll(offset + 3899999);
        TEST_ASSERT_TRUE(healthy.connected());
        healthy.poll(offset + 3900000);
        TEST_ASSERT_FALSE(healthy.connected());

        SRXL2::Link broken;
        connectFast(broken, offset);
        feed(broken, broadcast400, sizeof(broadcast400), offset + 1900000);
        uint8_t wrong[sizeof(esc)];
        std::memcpy(wrong, esc, sizeof(wrong));
        wrong[3] = 0x22;
        updateCrc(wrong, sizeof(wrong));
        feed(broken, wrong, sizeof(wrong), offset + 1910000);
        std::memcpy(wrong, esc, sizeof(wrong));
        wrong[21] ^= 1;
        feed(broken, wrong, sizeof(wrong), offset + 1920000);
        const uint8_t noise[] = {0, 0xA6, 0x80, 0xFF, 0xA6, 0x80, 4, 0};
        feed(broken, noise, sizeof(noise), offset + 2032799);
        broken.poll(offset + 2032799);
        TEST_ASSERT_TRUE(broken.connected());
        broken.poll(offset + 2032800);
        TEST_ASSERT_FALSE(broken.connected());
        TEST_ASSERT_EQUAL_UINT32(115200, broken.baudRate());
    }
}

void test_fast_peer_expiry_with_continuous_rf_recovers_after_tx_or_reply_grant()
{
    for (bool duringTx : {false, true})
    {
        SRXL2::Link link;
        connectFast(link);
        link.setControlPermission(true);
        link.setThrottle(992, 52800);
        SRXL2::Packet packet{};
        TEST_ASSERT_TRUE(link.nextPacket(52800, packet));
        link.transmitted(53200);
        feed(link, esc, sizeof(esc), 55000);
        for (uint32_t now = 102800; now < 2028000; now += 50000)
        {
            link.setThrottle(1811, now);
            TEST_ASSERT_TRUE(link.nextPacket(now, packet));
            TEST_ASSERT_EQUAL_HEX16(0xD554, channel(packet));
            link.transmitted(now + 400);
        }
        const uint32_t send = duringTx ? 2054900 : 2054000;
        link.setThrottle(1811, send);
        TEST_ASSERT_TRUE(link.nextPacket(send, packet));
        TEST_ASSERT_EQUAL_HEX8(0x40, packet.bytes[4]);
        const uint32_t stop = send + 400;
        if (!duringTx) link.transmitted(stop);
        TEST_ASSERT_FALSE(link.nextPacket(2055000, packet));
        TEST_ASSERT_FALSE(link.connected());
        TEST_ASSERT_FALSE(link.telemetry(2055000).voltage.valid);
        TEST_ASSERT_EQUAL_UINT32(400000, link.baudRate());
        if (duringTx) link.transmitted(stop);
        TEST_ASSERT_FALSE(link.nextPacket(stop + 49999, packet));
        TEST_ASSERT_EQUAL_UINT32(400000, link.baudRate());
        link.poll(stop + 50000);
        TEST_ASSERT_EQUAL_UINT32(115200, link.baudRate());
        TEST_ASSERT_FALSE(link.nextPacket(stop + 51000, packet));
        link.baudApplied(stop + 51000);
        feed(link, hello400, sizeof(hello400), stop + 51200);
        TEST_ASSERT_TRUE(link.nextPacket(stop + 51374, packet));
        TEST_ASSERT_EQUAL(0, packet.bytes[6]);
        link.transmitted(stop + 52590);
        link.setThrottle(1811, stop + 53000);
        TEST_ASSERT_TRUE(link.nextPacket(stop + 72590, packet));
        TEST_ASSERT_EQUAL_HEX16(0x8000, channel(packet));
    }
}

void test_fast_expiry_and_permission_loss_preserve_tx_and_reply_ownership()
{
    for (bool expiry : {false, true})
    {
        SRXL2::Link link;
        connectFast(link);
        link.setControlPermission(true);
        link.setThrottle(992, 52800);
        SRXL2::Packet packet{};
        TEST_ASSERT_TRUE(link.nextPacket(52800, packet));
        link.setThrottle(1811, 53000);
        const uint32_t revoke = expiry ? 153000 : 53100;
        if (!expiry) link.setControlPermission(false);
        link.poll(revoke);
        TEST_ASSERT_FALSE(link.nextPacket(revoke, packet));
        const uint32_t stop = revoke + 400;
        link.transmitted(stop);
        TEST_ASSERT_FALSE(link.nextPacket(stop + 19999, packet));
        TEST_ASSERT_TRUE(link.nextPacket(stop + 20000, packet));
        TEST_ASSERT_EQUAL(1, packet.bytes[3]);
        TEST_ASSERT_EQUAL_HEX16(0x8000, channel(packet));
        link.transmitted(stop + 20400);
        link.setControlPermission(true);
        link.setThrottle(1811, stop + 21000);
        TEST_ASSERT_TRUE(link.nextPacket(stop + 40000, packet));
        TEST_ASSERT_EQUAL_HEX16(0x8000, channel(packet));
    }
}

void test_fast_sample_and_permission_expiry_during_reply_grant()
{
    for (bool expiry : {false, true})
    {
        SRXL2::Link link;
        connectFast(link);
        link.setControlPermission(true);
        link.setThrottle(992, 52800);
        SRXL2::Packet packet{};
        TEST_ASSERT_TRUE(link.nextPacket(52800, packet));
        link.transmitted(53200);
        feed(link, esc, sizeof(esc), 55000);
        link.setThrottle(992, 153500);
        link.setThrottle(1811, 153501);
        TEST_ASSERT_TRUE(link.nextPacket(252800, packet));
        TEST_ASSERT_EQUAL_HEX16(0xD554, channel(packet));
        TEST_ASSERT_EQUAL_HEX8(0x40, packet.bytes[4]);
        link.transmitted(253200);
        if (!expiry) link.setControlPermission(false);
        link.poll(253501);
        link.setControlPermission(true);
        link.setThrottle(1811, 253502);
        TEST_ASSERT_FALSE(link.nextPacket(273199, packet));
        TEST_ASSERT_TRUE(link.nextPacket(273200, packet));
        TEST_ASSERT_EQUAL_HEX16(0x8000, channel(packet));
    }
}

void test_fast_rehandshake_requires_quiet_recovery_and_latches_unsuccessful_attempt()
{
    for (bool successful : {false, true})
    {
        for (unsigned trigger = 0; trigger < 3; ++trigger)
        {
            SRXL2::Link link;
            connectFast(link);
            link.setControlPermission(true);
            link.setThrottle(992, 52800);
            SRXL2::Packet packet{};
            TEST_ASSERT_TRUE(link.nextPacket(52800, packet));
            link.transmitted(53200);
            if (successful) feed(link, esc, sizeof(esc), 55000);
            link.setThrottle(1811, 74000);
            TEST_ASSERT_TRUE(link.nextPacket(74000, packet));
            link.transmitted(74400);
            uint8_t broadcastReply[sizeof(esc)];
            std::memcpy(broadcastReply, esc, sizeof(broadcastReply));
            broadcastReply[3] = 0xFF;
            updateCrc(broadcastReply, sizeof(broadcastReply));
            if (trigger == 0) feed(link, announce400, sizeof(announce400), 75000);
            else if (trigger == 1) feed(link, hello400, sizeof(hello400), 75000);
            else feed(link, broadcastReply, sizeof(broadcastReply), 75000);
            TEST_ASSERT_FALSE(link.connected());
            TEST_ASSERT_FALSE(link.telemetry(75000).voltage.valid);
            TEST_ASSERT_FALSE(link.nextPacket(124399, packet));
            TEST_ASSERT_EQUAL_UINT32(400000, link.baudRate());
            link.poll(124400);
            TEST_ASSERT_EQUAL_UINT32(115200, link.baudRate());
            TEST_ASSERT_FALSE(link.nextPacket(124600, packet));
            link.baudApplied(124600);
            feed(link, hello400, sizeof(hello400), 124800);
            TEST_ASSERT_TRUE(link.nextPacket(124974, packet));
            TEST_ASSERT_EQUAL(successful ? 1 : 0, packet.bytes[6]);
            link.transmitted(126190);
            if (successful) link.baudApplied(126190);
            link.setThrottle(1811, 146190);
            TEST_ASSERT_TRUE(link.nextPacket(146190, packet));
            TEST_ASSERT_EQUAL_HEX16(0x8000, channel(packet));
            link.transmitted(146590);
            if (!successful)
            {
                // More resets from the peer cannot undo the failed-fast latch.
                for (unsigned i = 0; i < 2; ++i)
                {
                    const uint32_t now = 170000 + i * 25000;
                    feed(link, hello400, sizeof(hello400), now);
                    TEST_ASSERT_TRUE(link.nextPacket(now + 174, packet));
                    TEST_ASSERT_EQUAL(0, packet.bytes[6]);
                    link.transmitted(now + 1390);
                }
                link.reset(0x12345678, 220000, false, true);
                feed(link, hello400, sizeof(hello400), 251400);
                TEST_ASSERT_TRUE(link.nextPacket(251600, packet));
                TEST_ASSERT_EQUAL(1, packet.bytes[6]);
            }
        }
    }
}

void test_fresh_announcement_during_fast_final_tx_finishes_before_recovery()
{
    SRXL2::Link link;
    link.reset(0x12345678, 0, false, true);
    feed(link, hello400, sizeof(hello400), 31400);
    SRXL2::Packet packet{};
    TEST_ASSERT_TRUE(link.nextPacket(31600, packet));
    feed(link, announce400, sizeof(announce400), 31700);
    TEST_ASSERT_FALSE(link.nextPacket(32000, packet));
    TEST_ASSERT_EQUAL_UINT32(115200, link.baudRate());
    link.transmitted(32800);
    TEST_ASSERT_EQUAL_UINT32(400000, link.baudRate());
    link.baudApplied(32800);
    TEST_ASSERT_FALSE(link.connected());
    link.poll(82799);
    TEST_ASSERT_EQUAL_UINT32(400000, link.baudRate());
    link.poll(82800);
    TEST_ASSERT_EQUAL_UINT32(115200, link.baudRate());
    link.baudApplied(82800);
    feed(link, hello400, sizeof(hello400), 83000);
    TEST_ASSERT_TRUE(link.nextPacket(83174, packet));
    TEST_ASSERT_EQUAL(0, packet.bytes[6]);
}

void test_unapplied_fast_reply_cannot_mark_attempt_successful()
{
    SRXL2::Link link;
    link.reset(0x12345678, 0, false, true);
    feed(link, hello400, sizeof(hello400), 31400);
    SRXL2::Packet packet{};
    TEST_ASSERT_TRUE(link.nextPacket(31600, packet));
    link.transmitted(32800);
    feed(link, esc, sizeof(esc), 33000);
    feed(link, hello, sizeof(hello), 33100); // A buffered addressed duplicate cannot restart pending fast mode.
    TEST_ASSERT_EQUAL_UINT32(400000, link.baudRate());
    link.baudApplied(40000);
    TEST_ASSERT_TRUE(link.connected());
    TEST_ASSERT_FALSE(link.telemetry(40000).voltage.valid);
    feed(link, announce400, sizeof(announce400), 41000);
    link.poll(82800);
    TEST_ASSERT_EQUAL_UINT32(115200, link.baudRate());
    link.baudApplied(82800);
    feed(link, hello400, sizeof(hello400), 83000);
    TEST_ASSERT_TRUE(link.nextPacket(83174, packet));
    TEST_ASSERT_EQUAL(0, packet.bytes[6]);
}

void test_fast_negotiation_waits_for_final_tx_and_baud_application()
{
    SRXL2::Link link;
    link.reset(0x12345678, 0, false, true);
    SRXL2::Packet packet{};
    const uint8_t request400[] = {0xA6,0x21,14,0x21,0x40,10,1,3,0x78,0x56,0x34,0x12,0xB6,0x31};
    TEST_ASSERT_TRUE(link.nextPacket(50000, packet));
    TEST_ASSERT_EQUAL_UINT8_ARRAY(request400, packet.bytes, sizeof(request400));
    link.transmitted(51216);
    feed(link, hello400, sizeof(hello400), 51400);
    TEST_ASSERT_TRUE(link.nextPacket(51574, packet));
    TEST_ASSERT_EQUAL_HEX8(0xFF, packet.bytes[4]);
    TEST_ASSERT_EQUAL_HEX8(1, packet.bytes[6]);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(broadcast400, packet.bytes, sizeof(broadcast400));
    TEST_ASSERT_EQUAL_UINT32(115200, link.baudRate());
    const uint32_t finalEndUs = 52790;
    link.transmitted(finalEndUs);
    TEST_ASSERT_EQUAL_UINT32(400000, link.baudRate());
    TEST_ASSERT_FALSE(link.connected());
    link.setControlPermission(true);
    link.setThrottle(992, finalEndUs + 1000); // An unapplied rate cannot accept the arming neutral.
    TEST_ASSERT_FALSE(link.nextPacket(finalEndUs + 20000, packet));
    link.baudApplied(finalEndUs);
    TEST_ASSERT_TRUE(link.connected());
    link.setThrottle(1811, finalEndUs + 20000);
    TEST_ASSERT_TRUE(link.nextPacket(finalEndUs + 20000, packet));
    TEST_ASSERT_EQUAL_HEX16(0x8000, channel(packet));
}

void test_baud_selection_requires_local_support_and_exact_peer_flag()
{
    for (bool local : {false, true})
    {
        for (uint8_t flag : {uint8_t(0), uint8_t(1), uint8_t(2), uint8_t(0x81)})
        {
            SRXL2::Link link;
            link.reset(0x12345678, 0, false, local);
            uint8_t reply[sizeof(hello)];
            std::memcpy(reply, hello, sizeof(reply));
            reply[6] = flag;
            updateCrc(reply, sizeof(reply));
            feed(link, reply, sizeof(reply), 31400);
            SRXL2::Packet packet{};
            TEST_ASSERT_TRUE(link.nextPacket(31600, packet));
            const uint8_t expected = local && flag == 1 ? 1 : 0;
            TEST_ASSERT_EQUAL(expected, link.commonBaudMask());
            TEST_ASSERT_EQUAL(expected, packet.bytes[6]);
            link.transmitted(32800);
            TEST_ASSERT_EQUAL_UINT32(expected ? 400000 : 115200, link.baudRate());
            TEST_ASSERT_EQUAL(!expected, link.connected());
        }
    }
}

void test_capability_intersection_ignores_invalid_frames_and_cannot_be_raised()
{
    for (unsigned invalid = 0; invalid < 4; ++invalid)
    {
        SRXL2::Link link;
        link.reset(0x12345678, 0, false, true);
        feed(link, hello400, sizeof(hello400), 31000);
        uint8_t bad[sizeof(hello)];
        std::memcpy(bad, hello, sizeof(bad));
        if (invalid == 0) bad[13] ^= 1;
        else if (invalid == 1) { bad[2] = 13; updateCrc(bad, 13); }
        else { bad[invalid == 2 ? 3 : 4] = 0x22; updateCrc(bad, sizeof(bad)); }
        feed(link, bad, sizeof(bad), 31100);
        TEST_ASSERT_EQUAL(1, link.commonBaudMask());
        SRXL2::Packet packet{};
        TEST_ASSERT_TRUE(link.nextPacket(34000, packet));
        TEST_ASSERT_EQUAL(1, packet.bytes[6]);
    }
    SRXL2::Link link;
    link.reset(0x12345678, 0, false, true);
    feed(link, hello400, sizeof(hello400), 31000);
    feed(link, hello, sizeof(hello), 31100);
    feed(link, hello400, sizeof(hello400), 31200);
    TEST_ASSERT_EQUAL(0, link.commonBaudMask());
    SRXL2::Packet packet{};
    TEST_ASSERT_TRUE(link.nextPacket(31400, packet));
    TEST_ASSERT_EQUAL(0, packet.bytes[6]);
    link.transmitted(32616);
    TEST_ASSERT_EQUAL_UINT32(115200, link.baudRate());
    TEST_ASSERT_TRUE(link.connected());
}

void test_fast_final_selection_is_frozen_against_buffered_duplicate()
{
    SRXL2::Link link;
    link.reset(0x12345678, 0, false, true);
    feed(link, hello400, sizeof(hello400), 31400);
    SRXL2::Packet packet{};
    TEST_ASSERT_TRUE(link.nextPacket(31600, packet));
    feed(link, hello, sizeof(hello), 31700);
    TEST_ASSERT_EQUAL_UINT32(115200, link.baudRate());
    TEST_ASSERT_FALSE(link.nextPacket(32000, packet));
    link.transmitted(32800);
    TEST_ASSERT_EQUAL_UINT32(400000, link.baudRate());
    link.baudApplied(32800);
    TEST_ASSERT_TRUE(link.connected());
}

void test_fast_announcement_capability_survives_ack_and_startup_handoff()
{
    for (bool duplicate : {false, true})
    {
        SRXL2::Link early;
        early.reset(0x12345678, 0, true, true);
        feed(early, announce400, sizeof(announce400), 31400);
        SRXL2::Packet packet{};
        TEST_ASSERT_TRUE(early.nextPacket(31600, packet));
        TEST_ASSERT_EQUAL(0x40, packet.bytes[4]);
        TEST_ASSERT_EQUAL(1, packet.bytes[6]);
        early.transmitted(32800);
        if (duplicate) feed(early, hello400, sizeof(hello400), 33000);
        TEST_ASSERT_FALSE(early.nextPacket(53000, packet));
        SRXL2::Link running;
        running.reset(0x12345678, 33000, false, true);
        running.finishStartupDiscovery(early.commonBaudMask());
        TEST_ASSERT_TRUE(running.nextPacket(33200, packet));
        TEST_ASSERT_EQUAL_UINT8_ARRAY(broadcast400, packet.bytes, sizeof(broadcast400));
    }
    for (uint8_t mask : {uint8_t(0), uint8_t(2), uint8_t(0x81)})
    {
        SRXL2::Link link;
        link.reset(0x12345678, 0, false, true);
        link.finishStartupDiscovery(mask);
        SRXL2::Packet packet{};
        TEST_ASSERT_TRUE(link.nextPacket(200, packet));
        TEST_ASSERT_EQUAL(0, packet.bytes[6]);
    }
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

void test_captured_firma_announcements_finalize_discovery_at_115200()
{
    // Nine copies captured from the Firma 85A before the receiver's first TX.
    const uint8_t announce[] = {0xA6,0x21,14,0x40,0,10,0,0,0,0,0,1,0x38,0x4E};
    const uint8_t acknowledge[] = {0xA6,0x21,14,0x21,0x40,10,0,3,0xCF,0x27,0x4D,6,0x5C,0x21};
    const uint8_t broadcast[] = {0xA6,0x21,14,0x21,0xFF,10,0,3,0xCF,0x27,0x4D,6,0x3E,0xBE};
    for (bool reply : {false, true})
    {
        SRXL2::Link link;
        link.reset(0x064D27CF, 787824);
        for (unsigned i = 0; i < 9; ++i) feed(link, announce, sizeof(announce), 910846);
        SRXL2::Packet packet{};
        TEST_ASSERT_FALSE(link.connected());
        TEST_ASSERT_FALSE(link.nextPacket(911019, packet));
        TEST_ASSERT_TRUE(link.nextPacket(911020, packet));
        TEST_ASSERT_EQUAL_UINT8_ARRAY(acknowledge, packet.bytes, sizeof(acknowledge));
        link.transmitted(912236);
        feed(link, acknowledge, sizeof(acknowledge), 912236); // Local echo cannot end the reply wait.
        TEST_ASSERT_FALSE(link.connected());
        feed(link, announce, sizeof(announce), 912436); // A buffered duplicate is not an addressed reply.
        TEST_ASSERT_FALSE(link.nextPacket(912610, packet));
        uint32_t finalUs = 932236;
        if (reply)
        {
            feed(link, hello, sizeof(hello), 912636);
            finalUs = 912810;
        }
        // The startup announcement is sufficient discovery data even without a duplicate reply.
        TEST_ASSERT_FALSE(link.nextPacket(finalUs - 1, packet));
        TEST_ASSERT_TRUE(link.nextPacket(finalUs, packet));
        TEST_ASSERT_EQUAL_UINT8_ARRAY(broadcast, packet.bytes, sizeof(broadcast));
        link.transmitted(finalUs + 1216);
        TEST_ASSERT_TRUE(link.connected());
        TEST_ASSERT_FALSE(link.nextPacket(finalUs + 21215, packet));
        TEST_ASSERT_TRUE(link.nextPacket(finalUs + 21216, packet));
        TEST_ASSERT_EQUAL_HEX8(0xCD, packet.bytes[1]);
        TEST_ASSERT_EQUAL(1, packet.bytes[3]); // No RF permission: failsafe, neutral throttle.
        TEST_ASSERT_EQUAL_HEX16(0x8000, channel(packet));
        link.transmitted(finalUs + 22606);
        feed(link, announce, sizeof(announce), finalUs + 23000);
        TEST_ASSERT_TRUE(link.nextPacket(finalUs + 23174, packet));
        TEST_ASSERT_EQUAL_UINT8_ARRAY(broadcast, packet.bytes, sizeof(broadcast)); // Running master re-finalizes directly.
    }
}

void test_local_echo_split_at_tx_done_preserves_immediate_hello()
{
    // UID 107 has an independently calculated discovery CRC ending in A6.
    const uint8_t request[] = {0xA6,0x21,14,0x21,0x40,10,0,3,107,0,0,0,0x64,0xA6};
    for (unsigned split = 0; split <= sizeof(request); ++split)
    {
        for (uint32_t delay : {200u, 4000u})
        {
          for (bool afterTxDone : {false, true})
          {
            SRXL2::Link link;
            link.reset(107, 0);
            SRXL2::Packet packet{};
            TEST_ASSERT_TRUE(link.nextPacket(50000, packet));
            TEST_ASSERT_EQUAL_UINT8_ARRAY(request, packet.bytes, sizeof(request));
            if (afterTxDone) link.transmitted(51216);
            feed(link, request, split, afterTxDone ? 51216 : 51100);
            if (!afterTxDone) link.transmitted(51216);
            const uint32_t replyTime = 51216 + delay;
            feed(link, request + split, sizeof(request) - split, replyTime);
            TEST_ASSERT_FALSE(link.nextPacket(replyTime, packet)); // Echo is not an ESC reply.
            feed(link, hello, sizeof(hello), replyTime);
            TEST_ASSERT_FALSE(link.nextPacket(replyTime + 173, packet));
            TEST_ASSERT_TRUE(link.nextPacket(replyTime + 174, packet));
            TEST_ASSERT_EQUAL_HEX8(0xFF, packet.bytes[4]); // Broadcast acknowledges the ESC hello.
          }
        }
    }
}

void test_buffered_genuine_reply_survives_callback_gap_after_complete_echo()
{
    for (unsigned split = 1; split < sizeof(hello); ++split)
    {
        SRXL2::Link link;
        link.reset(107, 0);
        SRXL2::Packet packet{};
        TEST_ASSERT_TRUE(link.nextPacket(50000, packet));
        const SRXL2::Packet echo = packet;
        link.transmitted(51216);
        feed(link, echo.bytes, echo.length, 51400);
        feed(link, hello, split, 51400);
        feed(link, hello + split, sizeof(hello) - split, 55400);
        TEST_ASSERT_FALSE(link.nextPacket(55573, packet));
        TEST_ASSERT_TRUE(link.nextPacket(55574, packet));
        TEST_ASSERT_EQUAL_HEX8(0xFF, packet.bytes[4]);
    }
}

void test_startup_neutral_release_and_failsafe_vector()
{
    SRXL2::Link link;
    connect(link);
    SRXL2::Packet packet{};
    const uint8_t guarded[] = {0xA6,0xCD,0x10,0,0x40,100,0,0,1,0,0,0,0,0x80,0x90,0x9A};
    link.setControlPermission(true);
    link.setThrottle(1811, 53000);
    TEST_ASSERT_TRUE(link.nextPacket(53000, packet));
    TEST_ASSERT_EQUAL_UINT8_ARRAY(guarded, packet.bytes, 16);
    link.transmitted(54400);
    feed(link, esc, sizeof(esc), 55000);
    link.setThrottle(992, 56000);
    TEST_ASSERT_TRUE(link.nextPacket(73000, packet));
    TEST_ASSERT_EQUAL(0, packet.bytes[3]);
    TEST_ASSERT_EQUAL_HEX16(0x8000, channel(packet));
    link.transmitted(74400);
    link.setThrottle(1811, 75000);
    TEST_ASSERT_TRUE(link.nextPacket(93000, packet));
    TEST_ASSERT_EQUAL_HEX16(0xD554, channel(packet));
    link.transmitted(94400);
    link.setControlPermission(false);
    TEST_ASSERT_TRUE(link.nextPacket(95000, packet));
    TEST_ASSERT_EQUAL(1, packet.bytes[3]);
    TEST_ASSERT_EQUAL(0, packet.bytes[4]);
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
        TEST_ASSERT_EQUAL(0, packet.bytes[3]);
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
    TEST_ASSERT_TRUE(link.nextPacket(73000, packet));
    link.transmitted(74400);
    TEST_ASSERT_TRUE(link.nextPacket(156000, packet));
    TEST_ASSERT_EQUAL(1, packet.bytes[3]);
    TEST_ASSERT_EQUAL_HEX16(0x8000, channel(packet));
    link.transmitted(157400);
    feed(link, esc, sizeof(esc), 160000);
    link.setThrottle(1811, 166000);
    TEST_ASSERT_FALSE(link.nextPacket(166000, packet));
    TEST_ASSERT_TRUE(link.nextPacket(176000, packet));
    TEST_ASSERT_EQUAL_HEX16(0x8000, channel(packet));
}

void test_delayed_rf_start_uses_normal_neutral_data_after_startup_failsafe()
{
    SRXL2::Link link;
    connect(link);
    SRXL2::Packet packet{};
    for (uint32_t now = 53000; now < 5000000; now += 20000)
    {
        TEST_ASSERT_TRUE(link.nextPacket(now, packet));
        TEST_ASSERT_EQUAL_HEX8(0xCD, packet.bytes[1]);
        TEST_ASSERT_EQUAL(1, packet.bytes[3]);
        TEST_ASSERT_EQUAL(0, packet.bytes[4]); // Failsafe does not request telemetry.
        TEST_ASSERT_EQUAL_HEX16(0x8000, channel(packet));
        link.transmitted(now + 1390);
    }
    TEST_ASSERT_TRUE(link.connected());
    link.setControlPermission(true);
    link.setThrottle(968, 5000000); // Captured TX12 value is outside the arming band.
    TEST_ASSERT_FALSE(link.nextPacket(5000000, packet));
    TEST_ASSERT_TRUE(link.nextPacket(5013000, packet));
    TEST_ASSERT_EQUAL_HEX8(0xCD, packet.bytes[1]);
    TEST_ASSERT_EQUAL(0, packet.bytes[3]);
    TEST_ASSERT_EQUAL_HEX8(0x40, packet.bytes[4]);
    TEST_ASSERT_EQUAL_HEX16(0x8000, channel(packet));
    link.transmitted(5014390);
    feed(link, esc, sizeof(esc), 5015000);
    link.setThrottle(1811, 5033000);
    TEST_ASSERT_TRUE(link.nextPacket(5033000, packet));
    TEST_ASSERT_EQUAL_HEX16(0x8000, channel(packet)); // Still unarmed.
    link.transmitted(5034390);
    link.setThrottle(992, 5053000);
    TEST_ASSERT_TRUE(link.nextPacket(5053000, packet));
    TEST_ASSERT_EQUAL_HEX16(0x8000, channel(packet));
    link.transmitted(5054390);
    link.setThrottle(1811, 5073000);
    TEST_ASSERT_TRUE(link.nextPacket(5073000, packet));
    TEST_ASSERT_EQUAL_HEX16(0xD554, channel(packet));
}

void test_recovered_rf_sample_restarts_reply_timeout_before_idle_processing()
{
    SRXL2::Link link;
    connect(link);
    link.setControlPermission(true);
    link.setThrottle(992, 53000);
    SRXL2::Packet packet{};
    TEST_ASSERT_TRUE(link.nextPacket(53000, packet));
    link.transmitted(54390);
    feed(link, esc, sizeof(esc), 55000);
    // A fresh RF callback can run before the serial loop notices the stale gap.
    link.setThrottle(1811, 3055000);
    TEST_ASSERT_TRUE(link.nextPacket(3055000, packet));
    TEST_ASSERT_EQUAL_HEX8(0xCD, packet.bytes[1]);
    TEST_ASSERT_EQUAL(0, packet.bytes[3]);
    TEST_ASSERT_EQUAL_HEX8(0x40, packet.bytes[4]);
    TEST_ASSERT_EQUAL_HEX16(0x8000, channel(packet));
    TEST_ASSERT_TRUE(link.connected());
}

void test_esc_schedule_preserves_settling_and_reply_time_with_500hz_rf()
{
    for (bool reply : {false, true})
    {
        SRXL2::Link link;
        link.reset(0x12345678, 0);
        SRXL2::Packet packet{};
        TEST_ASSERT_TRUE(link.nextPacket(50000, packet));
        link.transmitted(51216);
        feed(link, hello, sizeof(hello), 51400);
        TEST_ASSERT_TRUE(link.nextPacket(51574, packet));
        link.transmitted(52790);
        link.setControlPermission(true);
        link.setThrottle(992, 53000);
        TEST_ASSERT_FALSE(link.nextPacket(53000, packet)); // Allow the ESC to apply the final handshake.
        for (uint32_t now = 55000; now < 72790; now += 2000)
        {
            link.setThrottle(1811, now);
            TEST_ASSERT_FALSE(link.nextPacket(now, packet));
        }
        TEST_ASSERT_FALSE(link.nextPacket(72789, packet));
        TEST_ASSERT_TRUE(link.nextPacket(72790, packet));
        TEST_ASSERT_EQUAL_HEX16(0xD554, channel(packet));
        TEST_ASSERT_EQUAL_HEX8(0x40, packet.bytes[4]);
        link.transmitted(74180);
        for (uint32_t now = 75000; now <= 91000; now += 2000)
        {
            if (reply && now == 79000) feed(link, esc, sizeof(esc), 78180);
            link.setThrottle(172, now);
            TEST_ASSERT_FALSE(link.nextPacket(now, packet));
        }
        const uint32_t next = reply ? 92790 : 94180;
        TEST_ASSERT_FALSE(link.nextPacket(next - 1, packet));
        TEST_ASSERT_TRUE(link.nextPacket(next, packet));
        TEST_ASSERT_EQUAL_HEX16(0x2AA0, channel(packet)); // Send the latest RF value once.
        TEST_ASSERT_EQUAL(0, packet.bytes[4]);
    }
}

void test_telemetry_polls_every_200ms_without_extending_rf_sample_deadline()
{
    SRXL2::Link link;
    connect(link);
    link.setControlPermission(true);
    link.setThrottle(992, 53000);
    SRXL2::Packet packet{};
    TEST_ASSERT_TRUE(link.nextPacket(53000, packet));
    TEST_ASSERT_EQUAL_HEX8(0x40, packet.bytes[4]);
    link.transmitted(54390);
    feed(link, esc, sizeof(esc), 55000);
    for (uint32_t now = 73000; now <= 253000; now += 20000)
    {
        link.setThrottle(992, now);
        TEST_ASSERT_TRUE(link.nextPacket(now, packet));
        TEST_ASSERT_EQUAL(now == 253000 ? 0x40 : 0, packet.bytes[4]);
        link.transmitted(now + 1390);
    }
    TEST_ASSERT_TRUE(link.nextPacket(353000, packet));
    TEST_ASSERT_EQUAL(1, packet.bytes[3]); // RF samples still expire at 100 ms.
    TEST_ASSERT_EQUAL(0, packet.bytes[4]);
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
    TEST_ASSERT_FALSE(link.nextPacket(63999, packet));
    TEST_ASSERT_FALSE(link.nextPacket(72999, packet));
    TEST_ASSERT_TRUE(link.nextPacket(73000, packet));
    TEST_ASSERT_EQUAL(0, packet.bytes[4]);
}

void test_control_coalesces_50hz_100hz_and_500hz_rf_on_20ms_esc_cadence()
{
    for (uint32_t period : {20000u, 10000u, 2000u})
    {
        for (bool released : {false, true})
        {
            SRXL2::Link link;
            connect(link);
            link.setControlPermission(true);
            link.setThrottle(released ? 992 : 1811, 53000);
            SRXL2::Packet packet{};
            TEST_ASSERT_TRUE(link.nextPacket(53000, packet));
            link.transmitted(54390);
            feed(link, esc, sizeof(esc), 54564);
            unsigned emitted = 0;
            for (unsigned sample = 1; sample <= 40000 / period; ++sample)
            {
                const uint32_t time = 53000 + sample * period;
                TEST_ASSERT_FALSE(link.nextPacket(time - 1, packet)); // No repeated cached RF value.
                link.setThrottle(sample % 2 ? 1811 : 172, time);
                if ((time - 53000) % 20000)
                {
                    TEST_ASSERT_FALSE(link.nextPacket(time, packet));
                    continue;
                }
                TEST_ASSERT_TRUE(link.nextPacket(time, packet));
                ++emitted;
                TEST_ASSERT_EQUAL(0, packet.bytes[3]);
                TEST_ASSERT_EQUAL_HEX16(released ? (sample % 2 ? 0xD554 : 0x2AA0) : 0x8000, channel(packet));
                TEST_ASSERT_FALSE(link.nextPacket(time + 1389, packet));
                link.transmitted(time + 1390);
                TEST_ASSERT_FALSE(link.nextPacket(time + 1564, packet));
            }
            TEST_ASSERT_EQUAL(2, emitted);
        }
    }
}

void test_1ms_rf_updates_coalesce_to_latest_after_tx_and_reply_grant()
{
    SRXL2::Link link;
    connect(link);
    link.setControlPermission(true);
    link.setThrottle(992, 53000);
    SRXL2::Packet packet{};
    TEST_ASSERT_TRUE(link.nextPacket(53000, packet));
    link.transmitted(54390);
    // Updates during a grant keep only the latest accepted throttle.
    link.setThrottle(172, 54000);
    link.setThrottle(1811, 55000);
    TEST_ASSERT_FALSE(link.nextPacket(55000, packet));
    feed(link, esc, sizeof(esc), 55000);
    TEST_ASSERT_FALSE(link.nextPacket(55173, packet));
    TEST_ASSERT_FALSE(link.nextPacket(55174, packet));
    link.setThrottle(172, 56000);
    TEST_ASSERT_FALSE(link.nextPacket(56000, packet)); // FIFO/shifter still owns the bus.
    TEST_ASSERT_FALSE(link.nextPacket(72999, packet));
    TEST_ASSERT_TRUE(link.nextPacket(73000, packet));
    TEST_ASSERT_EQUAL_HEX16(0x2AA0, channel(packet));
    link.setThrottle(1811, 74000);
    TEST_ASSERT_FALSE(link.nextPacket(74000, packet));
    link.transmitted(74390);
    link.setThrottle(992, 76000);
    TEST_ASSERT_FALSE(link.nextPacket(92999, packet));
    TEST_ASSERT_TRUE(link.nextPacket(93000, packet));
    TEST_ASSERT_EQUAL_HEX16(0x8000, channel(packet));
    link.transmitted(94390);
    TEST_ASSERT_FALSE(link.nextPacket(95000, packet)); // Coalesced samples were consumed once.
}

void test_permission_loss_schedules_neutral_as_soon_as_bus_is_free()
{
    SRXL2::Link link;
    connect(link);
    link.setControlPermission(true);
    link.setThrottle(992, 53000);
    SRXL2::Packet packet{};
    TEST_ASSERT_TRUE(link.nextPacket(53000, packet));
    link.transmitted(54390);
    feed(link, esc, sizeof(esc), 54564);
    link.setThrottle(1811, 73000);
    TEST_ASSERT_TRUE(link.nextPacket(73000, packet));
    link.setControlPermission(false);
    TEST_ASSERT_FALSE(link.nextPacket(73100, packet));
    link.transmitted(74390);
    TEST_ASSERT_FALSE(link.nextPacket(74563, packet));
    TEST_ASSERT_TRUE(link.nextPacket(74564, packet));
    TEST_ASSERT_EQUAL(1, packet.bytes[3]);
    TEST_ASSERT_EQUAL_HEX16(0x8000, channel(packet));
    link.transmitted(75954);
    TEST_ASSERT_FALSE(link.nextPacket(94563, packet));
    TEST_ASSERT_TRUE(link.nextPacket(94564, packet)); // Inhibited output repeats every 20 ms.
}

void test_rearming_cannot_use_safety_priority_to_exceed_control_cadence()
{
    SRXL2::Link link;
    connect(link);
    link.setControlPermission(true);
    link.setThrottle(992, 53000);
    SRXL2::Packet packet{};
    TEST_ASSERT_TRUE(link.nextPacket(53000, packet));
    link.transmitted(54390);
    feed(link, esc, sizeof(esc), 55000);
    link.setThrottle(1811, 73000);
    TEST_ASSERT_TRUE(link.nextPacket(73000, packet));
    link.setControlPermission(false);
    link.transmitted(74390);
    link.setControlPermission(true);
    link.setThrottle(992, 75000);
    link.setThrottle(1811, 76000);
    TEST_ASSERT_FALSE(link.nextPacket(76000, packet));
    TEST_ASSERT_FALSE(link.nextPacket(92999, packet));
    TEST_ASSERT_TRUE(link.nextPacket(93000, packet));
    TEST_ASSERT_EQUAL_HEX16(0xD554, channel(packet));
}

void test_missed_rf_sends_fade_without_refreshing_authorized_sample()
{
    SRXL2::Link link;
    connect(link);
    link.setControlPermission(true);
    link.setThrottle(992, 53000);
    SRXL2::Packet packet{};
    TEST_ASSERT_TRUE(link.nextPacket(53000, packet));
    link.transmitted(54390);
    feed(link, esc, sizeof(esc), 54564);
    link.setThrottle(1811, 73000);
    TEST_ASSERT_TRUE(link.nextPacket(73000, packet));
    link.transmitted(74390);
    link.missedFrame();
    TEST_ASSERT_TRUE(link.nextPacket(113000, packet));
    const uint8_t fade[] = {0xA6,0xCD,14,0,0,100,0,0,0,0,0,0,0x62,0xE8};
    TEST_ASSERT_EQUAL(14, packet.length);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(fade, packet.bytes, sizeof(fade));
    link.transmitted(114216);
    link.missedFrame();
    TEST_ASSERT_TRUE(link.nextPacket(153000, packet));
    TEST_ASSERT_EQUAL(14, packet.length);
    TEST_ASSERT_EQUAL(0, packet.bytes[4]); // The 200 ms telemetry poll is not due yet.
    link.transmitted(154216);
    feed(link, esc, sizeof(esc), 154400);
    link.missedFrame();
    TEST_ASSERT_TRUE(link.nextPacket(173000, packet)); // 100 ms since the authorized sample.
    TEST_ASSERT_EQUAL(16, packet.length);
    TEST_ASSERT_EQUAL(1, packet.bytes[3]);
    TEST_ASSERT_EQUAL_HEX16(0x8000, channel(packet));
}

void test_fresh_pending_control_takes_precedence_over_fade()
{
    SRXL2::Link link;
    connect(link);
    link.setControlPermission(true);
    link.setThrottle(992, 53000);
    SRXL2::Packet packet{};
    TEST_ASSERT_TRUE(link.nextPacket(53000, packet));
    link.setThrottle(1811, 54000);
    link.missedFrame(); // Cannot replace the fresh update while TX/grant is busy.
    link.transmitted(54390);
    feed(link, esc, sizeof(esc), 54564);
    TEST_ASSERT_FALSE(link.nextPacket(54738, packet));
    TEST_ASSERT_TRUE(link.nextPacket(73000, packet));
    TEST_ASSERT_EQUAL(16, packet.length);
    TEST_ASSERT_EQUAL_HEX16(0xD554, channel(packet));
    link.transmitted(74390);
    link.missedFrame();
    link.setThrottle(172, 75000); // A fresh update replaces an already queued fade.
    TEST_ASSERT_FALSE(link.nextPacket(75000, packet));
    TEST_ASSERT_TRUE(link.nextPacket(93000, packet));
    TEST_ASSERT_EQUAL(16, packet.length);
    TEST_ASSERT_EQUAL_HEX16(0x2AA0, channel(packet));
}

void test_non_rc_rf_slots_get_keepalive_before_slave_timeout()
{
    for (bool released : {false, true})
    {
        SRXL2::Link link;
        connect(link);
        link.setControlPermission(true);
        link.setThrottle(released ? 992 : 1811, 53000);
        SRXL2::Packet packet{};
        TEST_ASSERT_TRUE(link.nextPacket(53000, packet));
        link.transmitted(54390);
        feed(link, esc, sizeof(esc), 54564);
        // DATA/SYNC slots yield neither a new control sample nor a missed callback.
        TEST_ASSERT_FALSE(link.nextPacket(92999, packet));
        TEST_ASSERT_TRUE(link.nextPacket(93000, packet));
        TEST_ASSERT_EQUAL(released ? 14 : 16, packet.length);
        TEST_ASSERT_EQUAL(0, packet.bytes[3]);
        if (!released) TEST_ASSERT_EQUAL_HEX16(0x8000, channel(packet));
        link.transmitted(94390);
        TEST_ASSERT_FALSE(link.nextPacket(132999, packet));
        TEST_ASSERT_TRUE(link.nextPacket(133000, packet));
        TEST_ASSERT_EQUAL(released ? 14 : 16, packet.length);
        link.transmitted(134390);
        TEST_ASSERT_FALSE(link.nextPacket(152999, packet));
        TEST_ASSERT_TRUE(link.nextPacket(153000, packet));
        TEST_ASSERT_EQUAL(16, packet.length);
        TEST_ASSERT_EQUAL(1, packet.bytes[3]);
        TEST_ASSERT_EQUAL_HEX16(0x8000, channel(packet));
    }
}

void test_rediscovery_waits_for_outstanding_reply_and_revokes_motion()
{
    for (bool reply : {false, true})
    {
        SRXL2::Link link;
        connect(link);
        link.setControlPermission(true);
        link.setThrottle(992, 53000);
        SRXL2::Packet packet{};
        TEST_ASSERT_TRUE(link.nextPacket(53000, packet));
        link.transmitted(54400);
        feed(link, esc, sizeof(esc), 55000);
        for (uint32_t now = 103000; now < 2052000; now += 50000)
        {
            link.setThrottle(1811, now);
            TEST_ASSERT_TRUE(link.nextPacket(now, packet));
            link.transmitted(now + 1390);
        }
        link.setControlPermission(true);
        link.setThrottle(992, 2052000);
        link.setThrottle(1811, 2052500);
        TEST_ASSERT_TRUE(link.nextPacket(2053000, packet));
        TEST_ASSERT_EQUAL_HEX8(0x40, packet.bytes[4]);
        TEST_ASSERT_EQUAL_HEX16(0xD554, channel(packet));
        link.transmitted(2054390);
        TEST_ASSERT_FALSE(link.nextPacket(2055000, packet));
        TEST_ASSERT_TRUE(link.connected());
        if (reply)
        {
            feed(link, esc, sizeof(esc), 2056000);
            TEST_ASSERT_TRUE(link.nextPacket(2063000, packet));
            TEST_ASSERT_EQUAL_HEX8(0xCD, packet.bytes[1]);
            TEST_ASSERT_EQUAL(1, packet.bytes[3]);
            TEST_ASSERT_EQUAL_HEX16(0x8000, channel(packet));
            TEST_ASSERT_TRUE(link.connected());
        }
        else
        {
            TEST_ASSERT_FALSE(link.nextPacket(2074389, packet));
            TEST_ASSERT_TRUE(link.nextPacket(2074390, packet));
            TEST_ASSERT_EQUAL_HEX8(0x21, packet.bytes[1]);
            TEST_ASSERT_FALSE(link.connected());
        }
    }
}

void test_esc_scaling_and_invalid_replacement()
{
    SRXL2::Link link;
    connect(link);
    feed(link, esc, sizeof(esc), 60000);
    auto values = link.telemetry(60000);
    TEST_ASSERT_EQUAL(60000, values.rpm.updatedUs);
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
    SRXL2::Packet packet{};
    TEST_ASSERT_FALSE(link.nextPacket(64499, packet));
    TEST_ASSERT_TRUE(link.nextPacket(64500, packet)); // Observed idle expires the partial frame at 2500 us.
    link.transmitted(65890);
    feed(link, esc, sizeof(esc), 72000);
    TEST_ASSERT_TRUE(link.telemetry(72000).voltage.valid);
    feed(link, battery, 9, 73000);
    feed(link, battery + 9, sizeof(battery) - 9, 73100);
    feed(link, cells, sizeof(cells), 73200);
    TEST_ASSERT_EQUAL(3000, link.telemetry(73200).batteryCurrent.value);
    TEST_ASSERT_EQUAL(4097, link.telemetry(73200).cells[0].value);
}

void test_parser_retains_frames_after_resynchronizing_at_declared_length()
{
    for (uint8_t length : {uint8_t(26), uint8_t(80)})
    {
        SRXL2::Link link;
        connect(link);
        const uint8_t noise[] = {0xA6, 0x80, length};
        feed(link, noise, sizeof(noise), 60000);
        feed(link, esc, sizeof(esc), 60100);
        feed(link, battery, sizeof(battery), 60200);
        feed(link, cells, sizeof(cells), 60300);
        feed(link, identity, sizeof(identity), 60400);
        const auto values = link.telemetry(60400);
        TEST_ASSERT_TRUE(values.voltage.valid);
        TEST_ASSERT_EQUAL(12340, values.voltage.value);
        TEST_ASSERT_TRUE(values.batteryCurrent.valid);
        TEST_ASSERT_EQUAL(3000, values.batteryCurrent.value);
        TEST_ASSERT_EQUAL(4097, values.cells[0].value);
        TEST_ASSERT_EQUAL(3, values.cellCount);
    }
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
    TEST_ASSERT_TRUE(link.nextPacket(63000, packet));
    link.transmitted(64390);
    for (uint32_t now = 113000; now < 2063000; now += 50000)
    {
        link.setThrottle(1811, now);
        TEST_ASSERT_TRUE(link.nextPacket(now, packet));
        link.transmitted(now + 1390);
    }
    link.setThrottle(1811, 2063000);
    TEST_ASSERT_TRUE(link.nextPacket(2063000, packet));
    TEST_ASSERT_FALSE(link.connected());
    TEST_ASSERT_FALSE(link.telemetry(2063000).voltage.valid);
    TEST_ASSERT_EQUAL(0x21, packet.bytes[1]);
    link.transmitted(2064200);
    feed(link, hello, sizeof(hello), 2064400);
    TEST_ASSERT_TRUE(link.nextPacket(2064600, packet));
    link.transmitted(2065800);
    link.setThrottle(1811, 2066000);
    TEST_ASSERT_FALSE(link.nextPacket(2066000, packet));
    TEST_ASSERT_TRUE(link.nextPacket(2085800, packet));
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

void test_expired_optional_measurements_do_not_revive_after_full_wrap()
{
    SRXL2::Link link;
    connect(link);
    feed(link, battery, sizeof(battery), 60000);
    feed(link, cells, sizeof(cells), 61000);
    TEST_ASSERT_TRUE(link.telemetry(61000).batteryCurrent.valid);
    TEST_ASSERT_FALSE(link.telemetry(5061000).batteryCurrent.valid);
    TEST_ASSERT_FALSE(link.telemetry(5061000).cells[0].valid);
    // The same counter values one full 32-bit cycle later must remain expired.
    TEST_ASSERT_FALSE(link.telemetry(61000).batteryCurrent.valid);
    TEST_ASSERT_FALSE(link.telemetry(61000).cells[0].valid);
    feed(link, battery, sizeof(battery), 62000);
    TEST_ASSERT_TRUE(link.telemetry(62000).batteryCurrent.valid);
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_fast_link_polls_safely_without_rf);
    RUN_TEST(test_fast_timeout_returns_to_default_after_master_silence);
    RUN_TEST(test_fast_health_accepts_unavailable_replies_but_ignores_echo_and_noise);
    RUN_TEST(test_fast_peer_expiry_with_continuous_rf_recovers_after_tx_or_reply_grant);
    RUN_TEST(test_fast_expiry_and_permission_loss_preserve_tx_and_reply_ownership);
    RUN_TEST(test_fast_sample_and_permission_expiry_during_reply_grant);
    RUN_TEST(test_fast_rehandshake_requires_quiet_recovery_and_latches_unsuccessful_attempt);
    RUN_TEST(test_fresh_announcement_during_fast_final_tx_finishes_before_recovery);
    RUN_TEST(test_unapplied_fast_reply_cannot_mark_attempt_successful);
    RUN_TEST(test_fast_negotiation_waits_for_final_tx_and_baud_application);
    RUN_TEST(test_baud_selection_requires_local_support_and_exact_peer_flag);
    RUN_TEST(test_capability_intersection_ignores_invalid_frames_and_cannot_be_raised);
    RUN_TEST(test_fast_final_selection_is_frozen_against_buffered_duplicate);
    RUN_TEST(test_fast_announcement_capability_survives_ack_and_startup_handoff);
    RUN_TEST(test_crc_and_nominal_throttle);
    RUN_TEST(test_handshake_vectors_and_tx_ownership);
    RUN_TEST(test_captured_firma_announcements_finalize_discovery_at_115200);
    RUN_TEST(test_local_echo_split_at_tx_done_preserves_immediate_hello);
    RUN_TEST(test_buffered_genuine_reply_survives_callback_gap_after_complete_echo);
    RUN_TEST(test_startup_neutral_release_and_failsafe_vector);
    RUN_TEST(test_neutral_band_boundaries_and_permission_recovery);
    RUN_TEST(test_stale_control_needs_new_neutral);
    RUN_TEST(test_delayed_rf_start_uses_normal_neutral_data_after_startup_failsafe);
    RUN_TEST(test_recovered_rf_sample_restarts_reply_timeout_before_idle_processing);
    RUN_TEST(test_esc_schedule_preserves_settling_and_reply_time_with_500hz_rf);
    RUN_TEST(test_telemetry_polls_every_200ms_without_extending_rf_sample_deadline);
    RUN_TEST(test_response_window_and_control_cadence);
    RUN_TEST(test_control_coalesces_50hz_100hz_and_500hz_rf_on_20ms_esc_cadence);
    RUN_TEST(test_1ms_rf_updates_coalesce_to_latest_after_tx_and_reply_grant);
    RUN_TEST(test_permission_loss_schedules_neutral_as_soon_as_bus_is_free);
    RUN_TEST(test_rearming_cannot_use_safety_priority_to_exceed_control_cadence);
    RUN_TEST(test_missed_rf_sends_fade_without_refreshing_authorized_sample);
    RUN_TEST(test_fresh_pending_control_takes_precedence_over_fade);
    RUN_TEST(test_non_rc_rf_slots_get_keepalive_before_slave_timeout);
    RUN_TEST(test_rediscovery_waits_for_outstanding_reply_and_revokes_motion);
    RUN_TEST(test_esc_scaling_and_invalid_replacement);
    RUN_TEST(test_smart_battery_endian_and_cells);
    RUN_TEST(test_parser_recovers_from_noise_bad_crc_and_partial_timeout);
    RUN_TEST(test_parser_retains_frames_after_resynchronizing_at_declared_length);
    RUN_TEST(test_wrong_master_and_disconnect_clear_data_and_motion);
    RUN_TEST(test_unavailable_fields_are_still_valid_bus_replies);
    RUN_TEST(test_timer_wrap);
    RUN_TEST(test_expired_optional_measurements_do_not_revive_after_full_wrap);
    run_adapter_tests();
    return UNITY_END();
}

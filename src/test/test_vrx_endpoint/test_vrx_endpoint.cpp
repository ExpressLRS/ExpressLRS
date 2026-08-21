#include <cstring>
#include <string>
#include <unity.h>

#include "VideoReceiverEndpoint.h"
#include "crsf_protocol.h"
#include "msp.h"

// Captures everything MSP::sendPacket writes, so we can assert on exact bytes
class CaptureStream final : public Stream
{
public:
    int available() override { return 0; }
    int read() override { return -1; }
    int peek() override { return -1; }
    void flush() override {}

    size_t write(uint8_t c) override { bytes += (char)c; return 1; }
    size_t write(const uint8_t *s, size_t l) override
    {
        for (size_t i = 0; i < l; i++) bytes += (char)s[i];
        return l;
    }

    std::string bytes;
};

// A SET_RTC message for 2026-01-01 12:34:56, MSPv2-encapsulated, addressed to
// the video receiver. See the frame table in the plan for the byte breakdown.
static uint8_t setRtcFrame[] = {
    0xEE, 0x11, 0x7C, 0x14, 0xEA,
    0x50, 0x00, 0x0E, 0x03, 0x06, 0x00,
    0x7E, 0x00, 0x01, 0x0C, 0x22, 0x38,
    0x92, 0x35
};

static void test_decodes_mspv2_command()
{
    mspPacket_t packet;
    const bool decoded = decodeEncapsulatedMsp((crsf_header_t *)setRtcFrame, &packet);

    TEST_ASSERT_TRUE(decoded);
    TEST_ASSERT_EQUAL(MSP_PACKET_COMMAND, packet.type);
    TEST_ASSERT_EQUAL_UINT16(0x030E, packet.function);
    TEST_ASSERT_EQUAL_UINT16(6, packet.payloadSize);
    TEST_ASSERT_EQUAL_UINT8(126, packet.payload[0]); // tm_year, 2026
    TEST_ASSERT_EQUAL_UINT8(0, packet.payload[1]);   // tm_mon, January
    TEST_ASSERT_EQUAL_UINT8(1, packet.payload[2]);
    TEST_ASSERT_EQUAL_UINT8(12, packet.payload[3]);
    TEST_ASSERT_EQUAL_UINT8(34, packet.payload[4]);
    TEST_ASSERT_EQUAL_UINT8(56, packet.payload[5]);
}

// The whole point of decoding rather than relaying verbatim: what lands on the
// backpack serial must be a native MSPv2 command frame the backpack will accept.
static void test_emits_native_mspv2_to_backpack()
{
    mspPacket_t packet;
    TEST_ASSERT_TRUE(decodeEncapsulatedMsp((crsf_header_t *)setRtcFrame, &packet));

    CaptureStream stream;
    TEST_ASSERT_TRUE(MSP::sendPacket(&packet, &stream));

    const uint8_t expected[] = {
        '$', 'X', '<',
        0x00, 0x0E, 0x03, 0x06, 0x00,
        0x7E, 0x00, 0x01, 0x0C, 0x22, 0x38,
        0x92
    };
    TEST_ASSERT_EQUAL_UINT32(sizeof(expected), stream.bytes.length());
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, stream.bytes.data(), sizeof(expected));
}

static void test_decodes_mspv1_command()
{
    // MSPv1 encapsulation: status 0x30, then size, then an 8-bit function
    uint8_t frame[] = {
        0xEE, 0x0B, 0x7C, 0x14, 0xEA,
        0x30, 0x02, 0x59, 0xAA, 0xBB,
        0x00, 0x00
    };

    mspPacket_t packet;
    TEST_ASSERT_TRUE(decodeEncapsulatedMsp((crsf_header_t *)frame, &packet));
    TEST_ASSERT_EQUAL_UINT16(0x59, packet.function);
    TEST_ASSERT_EQUAL_UINT16(2, packet.payloadSize);
    TEST_ASSERT_EQUAL_UINT8(0xAA, packet.payload[0]);
    TEST_ASSERT_EQUAL_UINT8(0xBB, packet.payload[1]);
}

static void test_rejects_non_msp_frame_type()
{
    uint8_t frame[sizeof(setRtcFrame)];
    memcpy(frame, setRtcFrame, sizeof(setRtcFrame));
    frame[2] = CRSF_FRAMETYPE_PARAMETER_WRITE;

    mspPacket_t packet;
    TEST_ASSERT_FALSE(decodeEncapsulatedMsp((crsf_header_t *)frame, &packet));
}

static void test_rejects_error_bit()
{
    uint8_t frame[sizeof(setRtcFrame)];
    memcpy(frame, setRtcFrame, sizeof(setRtcFrame));
    frame[5] |= 0x80; // error bit in the MSP status byte

    mspPacket_t packet;
    TEST_ASSERT_FALSE(decodeEncapsulatedMsp((crsf_header_t *)frame, &packet));
}

static void test_rejects_chunked_continuation()
{
    uint8_t frame[sizeof(setRtcFrame)];
    memcpy(frame, setRtcFrame, sizeof(setRtcFrame));
    frame[5] &= ~0x10; // clear start-of-frame; we do not reassemble chunks

    mspPacket_t packet;
    TEST_ASSERT_FALSE(decodeEncapsulatedMsp((crsf_header_t *)frame, &packet));
}

static void test_rejects_payload_overrunning_frame()
{
    uint8_t frame[sizeof(setRtcFrame)];
    memcpy(frame, setRtcFrame, sizeof(setRtcFrame));
    frame[9] = 0x40; // claim a 64-byte payload that cannot fit in this frame

    mspPacket_t packet;
    TEST_ASSERT_FALSE(decodeEncapsulatedMsp((crsf_header_t *)frame, &packet));
}

void setUp() {}
void tearDown() {}

int main(int, char **)
{
    UNITY_BEGIN();
    RUN_TEST(test_decodes_mspv2_command);
    RUN_TEST(test_emits_native_mspv2_to_backpack);
    RUN_TEST(test_decodes_mspv1_command);
    RUN_TEST(test_rejects_non_msp_frame_type);
    RUN_TEST(test_rejects_error_bit);
    RUN_TEST(test_rejects_chunked_continuation);
    RUN_TEST(test_rejects_payload_overrunning_frame);
    return UNITY_END();
}

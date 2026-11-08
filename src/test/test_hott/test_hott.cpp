#include <cstdint>
#include <vector>

#include <unity.h>

// SerialHoTT_TLM is receiver-only code
#define TARGET_RX 1
#include "CRSFRouter.h"
#include "common.h"

CRSFRouter crsfRouter;
connectionState_e connectionState = connected;
bool crsfBatterySensorDetected = false;
bool crsfBaroSensorDetected = false;

// crsf_protocol.h only provides htobe24 for non-Linux (firmware) builds
static inline uint32_t htobe24(uint32_t val)
{
    return ((val & 0xff) << 16) | (val & 0xff00) | ((val >> 16) & 0xff);
}

using std::min;
#include "../../src/rx-serial/SerialIO.cpp"
// the CRSF senders are private, the test calls them directly
#define private public
#include "../../src/rx-serial/SerialHoTT_TLM.cpp"
#undef private

class NullStream final : public Stream
{
public:
    int available() override { return 0; }
    int read() override { return -1; }
    int peek() override { return -1; }
    void flush() override {}
    size_t write(uint8_t) override { return 1; }
    size_t write(const uint8_t *, size_t len) override { return len; }
};

class MockConnector final : public CRSFConnector
{
public:
    MockConnector() { addDevice(CRSF_ADDRESS_RADIO_TRANSMITTER); }

    void forwardMessage(const crsf_header_t *message) override
    {
        const uint8_t *bytes = reinterpret_cast<const uint8_t *>(message);
        frames.emplace_back(bytes, bytes + message->frame_size + CRSF_FRAME_NOT_COUNTED_BYTES);
    }

    std::vector<std::vector<uint8_t>> frames;
};

static NullStream stream;
static MockConnector connector;

// frame_size counts type + payload + crc
static uint8_t payloadLen(const std::vector<uint8_t> &frame) { return frame[1] - 2; }

static void test_turbine_rpm_payload_size()
{
    SerialHoTT_TLM hott(stream, stream);
    hott.escIsTurbine = true;
    connector.frames.clear();
    hott.sendCRSFrpm(100000, ESC);

    TEST_ASSERT_EQUAL_UINT32(1, connector.frames.size());
    // source id + rpm, rpm max, fuel, fuel flow (4 x 24 bit)
    TEST_ASSERT_EQUAL_UINT8(1 + 4 * 3, payloadLen(connector.frames[0]));
}

static void test_turbine_temp_payload_size()
{
    SerialHoTT_TLM hott(stream, stream);
    hott.escIsTurbine = true;
    connector.frames.clear();
    hott.sendCRSFtemp(100000, ESC);

    TEST_ASSERT_EQUAL_UINT32(1, connector.frames.size());
    // source id + 5 temperatures + throttle + status (7 x 16 bit)
    TEST_ASSERT_EQUAL_UINT8(1 + 7 * 2, payloadLen(connector.frames[0]));
}

static void test_esc_payload_sizes()
{
    SerialHoTT_TLM hott(stream, stream);
    hott.escIsTurbine = false;
    connector.frames.clear();
    hott.sendCRSFrpm(100000, ESC);
    hott.sendCRSFtemp(100000, ESC);

    TEST_ASSERT_EQUAL_UINT32(2, connector.frames.size());
    TEST_ASSERT_EQUAL_UINT8(1 + 2 * 3, payloadLen(connector.frames[0]));
    TEST_ASSERT_EQUAL_UINT8(1 + 5 * 2, payloadLen(connector.frames[1]));
}

void setUp() {}
void tearDown() {}

int main(int argc, char **argv)
{
    crsfRouter.addConnector(&connector);
    UNITY_BEGIN();
    RUN_TEST(test_turbine_rpm_payload_size);
    RUN_TEST(test_turbine_temp_payload_size);
    RUN_TEST(test_esc_payload_sizes);
    UNITY_END();
    return 0;
}

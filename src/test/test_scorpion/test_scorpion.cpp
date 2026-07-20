#include <algorithm>
#include <array>
#include <cstdint>
#include <vector>

#include <unity.h>

#include "CRSFRouter.h"
#include "rx-serial/SerialScorpion_TLM.h"
#include "common.h"

CRSFRouter crsfRouter;
bool crsfBatterySensorDetected = false;

// Receiver serial implementations live in the application source tree, which
// PlatformIO does not link into individual native test programs by default.
using std::min;
#include "../../src/rx-serial/SerialIO.cpp"
#include "../../src/rx-serial/SerialScorpion_TLM.cpp"

static_assert(PROTOCOL_SCORPION_TLM == 10, "Unexpected Scorpion protocol value");
static_assert(PROTOCOL_SCORPION_TLM < 16, "Scorpion protocol does not fit persisted storage");

namespace
{
class MockStream : public Stream
{
public:
    int available() override { return static_cast<int>(input.size() - readPosition); }
    int read() override { return readPosition < input.size() ? input[readPosition++] : -1; }
    int peek() override { return readPosition < input.size() ? input[readPosition] : -1; }
    void flush() override {}

    size_t write(uint8_t value) override
    {
        output.push_back(value);
        return 1;
    }

    size_t write(const uint8_t *data, size_t length) override
    {
        output.insert(output.end(), data, data + length);
        return length;
    }

    std::vector<uint8_t> input;
    std::vector<uint8_t> output;

private:
    size_t readPosition = 0;
};

class MockConnector : public CRSFConnector
{
public:
    MockConnector()
    {
        addDevice(CRSF_ADDRESS_RADIO_TRANSMITTER);
    }

    void forwardMessage(const crsf_header_t *message) override
    {
        const uint8_t *bytes = reinterpret_cast<const uint8_t *>(message);
        frames.emplace_back(bytes, bytes + message->frame_size + CRSF_FRAME_NOT_COUNTED_BYTES);
    }

    std::vector<std::vector<uint8_t>> frames;
};

using ScorpionFrame = std::array<uint8_t, 22>;

ScorpionFrame goldenFrame()
{
    return {{
        0x55, 0x01, 0x16, 0x00,
        0x56, 0x34, 0x12,
        0x64,
        0xF4, 0x01,
        0xF4, 0x01,
        0x34, 0x12,
        0x4B,
        0xA0,
        0x52,
        0x56, 0x34,
        0xA5,
        0xF4, 0xC0
    }};
}

ScorpionFrame maximumFrame()
{
    return {{
        0x55, 0xFF, 0x16, 0xFF,
        0xFF, 0xFF, 0xFF,
        0xFF,
        0xFF, 0xFF,
        0xFF, 0xFF,
        0xFF, 0xFF,
        0xFF,
        0xFF,
        0xFF,
        0xFF, 0xFF,
        0xFF,
        0xC6, 0x64
    }};
}

ScorpionFrame embeddedSyncFrame()
{
    ScorpionFrame frame = goldenFrame();
    frame[7] = 0x55;
    frame[20] = 0x29;
    frame[21] = 0x2A;
    return frame;
}

struct Fixture
{
    Fixture()
        : serial(output, input)
    {
        crsfRouter.addConnector(&connector);
    }

    ~Fixture() { crsfRouter.removeConnector(&connector); }

    MockStream output;
    MockStream input;
    MockConnector connector;
    SerialScorpion_TLM serial;
};
}

class SerialScorpionTlmTestAccess
{
public:
    static void process(SerialScorpion_TLM &serial, const uint8_t *data, size_t length)
    {
        serial.processBytes(const_cast<uint8_t *>(data), static_cast<uint16_t>(length));
    }

    static void setLastReceivedByteMs(SerialScorpion_TLM &serial, uint32_t value) { serial.lastReceivedByteMs = value; }
    static bool partialFrameTimedOut(uint32_t now, uint32_t lastReceived)
    {
        return SerialScorpion_TLM::partialFrameTimedOut(now, lastReceived);
    }

    static uint8_t framePosition(const SerialScorpion_TLM &serial) { return serial.framePosition; }
    static uint32_t validFrameCount(const SerialScorpion_TLM &serial) { return serial.validFrameCount; }
    static uint32_t timestampMs(const SerialScorpion_TLM &serial) { return serial.decoded.timestampMs; }
    static uint16_t throttlePermille(const SerialScorpion_TLM &serial) { return serial.decoded.throttlePermille; }
    static uint16_t currentDeciAmps(const SerialScorpion_TLM &serial) { return serial.decoded.currentDeciAmps; }
    static uint16_t voltageDeciVolts(const SerialScorpion_TLM &serial) { return serial.decoded.voltageDeciVolts; }
    static uint16_t consumedMah(const SerialScorpion_TLM &serial) { return serial.decoded.consumedMah; }
    static uint16_t temperatureDeciCelsius(const SerialScorpion_TLM &serial) { return serial.decoded.temperatureDeciCelsius; }
    static uint16_t pwmPermille(const SerialScorpion_TLM &serial) { return serial.decoded.pwmPermille; }
    static uint16_t becMillivolts(const SerialScorpion_TLM &serial) { return serial.decoded.becMillivolts; }
    static uint32_t rpm(const SerialScorpion_TLM &serial) { return serial.decoded.rpm; }
    static uint8_t status(const SerialScorpion_TLM &serial) { return serial.decoded.status; }
};

void test_valid_frame_decodes_all_fields()
{
    Fixture fixture;
    const ScorpionFrame frame = goldenFrame();

    SerialScorpionTlmTestAccess::process(fixture.serial, frame.data(), frame.size());

    TEST_ASSERT_EQUAL_UINT32(1, SerialScorpionTlmTestAccess::validFrameCount(fixture.serial));
    TEST_ASSERT_EQUAL_HEX32(0x123456, SerialScorpionTlmTestAccess::timestampMs(fixture.serial));
    TEST_ASSERT_EQUAL_UINT16(500, SerialScorpionTlmTestAccess::throttlePermille(fixture.serial));
    TEST_ASSERT_EQUAL_UINT16(500, SerialScorpionTlmTestAccess::currentDeciAmps(fixture.serial));
    TEST_ASSERT_EQUAL_UINT16(500, SerialScorpionTlmTestAccess::voltageDeciVolts(fixture.serial));
    TEST_ASSERT_EQUAL_UINT16(0x1234, SerialScorpionTlmTestAccess::consumedMah(fixture.serial));
    TEST_ASSERT_EQUAL_UINT16(750, SerialScorpionTlmTestAccess::temperatureDeciCelsius(fixture.serial));
    TEST_ASSERT_EQUAL_UINT16(800, SerialScorpionTlmTestAccess::pwmPermille(fixture.serial));
    TEST_ASSERT_EQUAL_UINT16(8200, SerialScorpionTlmTestAccess::becMillivolts(fixture.serial));
    TEST_ASSERT_EQUAL_UINT32(66990, SerialScorpionTlmTestAccess::rpm(fixture.serial));
    TEST_ASSERT_EQUAL_HEX8(0xA5, SerialScorpionTlmTestAccess::status(fixture.serial));
    TEST_ASSERT_TRUE(crsfBatterySensorDetected);
    TEST_ASSERT_EQUAL_UINT8(0, SerialScorpionTlmTestAccess::framePosition(fixture.serial));
}

void test_incorrect_crc_is_rejected()
{
    Fixture fixture;
    ScorpionFrame frame = goldenFrame();
    frame[20] ^= 0x01;

    SerialScorpionTlmTestAccess::process(fixture.serial, frame.data(), frame.size());

    TEST_ASSERT_EQUAL_UINT32(0, SerialScorpionTlmTestAccess::validFrameCount(fixture.serial));
    TEST_ASSERT_FALSE(crsfBatterySensorDetected);
    TEST_ASSERT_TRUE(fixture.connector.frames.empty());
}

void test_noise_before_sync_is_ignored()
{
    Fixture fixture;
    const ScorpionFrame frame = goldenFrame();
    const uint8_t noise[] = {0x00, 0x7E, 0x54, 0xAA};

    SerialScorpionTlmTestAccess::process(fixture.serial, noise, sizeof(noise));
    SerialScorpionTlmTestAccess::process(fixture.serial, frame.data(), frame.size());

    TEST_ASSERT_EQUAL_UINT32(1, SerialScorpionTlmTestAccess::validFrameCount(fixture.serial));
}

void test_frame_can_be_split_across_calls()
{
    Fixture fixture;
    const ScorpionFrame frame = goldenFrame();

    SerialScorpionTlmTestAccess::process(fixture.serial, frame.data(), 9);
    TEST_ASSERT_EQUAL_UINT8(9, SerialScorpionTlmTestAccess::framePosition(fixture.serial));
    SerialScorpionTlmTestAccess::process(fixture.serial, frame.data() + 9, frame.size() - 9);

    TEST_ASSERT_EQUAL_UINT32(1, SerialScorpionTlmTestAccess::validFrameCount(fixture.serial));
}

void test_multiple_frames_in_one_call()
{
    Fixture fixture;
    const ScorpionFrame frame = goldenFrame();
    std::array<uint8_t, 44> frames;
    std::copy(frame.begin(), frame.end(), frames.begin());
    std::copy(frame.begin(), frame.end(), frames.begin() + frame.size());

    SerialScorpionTlmTestAccess::process(fixture.serial, frames.data(), frames.size());

    TEST_ASSERT_EQUAL_UINT32(2, SerialScorpionTlmTestAccess::validFrameCount(fixture.serial));
    TEST_ASSERT_EQUAL_UINT32(2, fixture.connector.frames.size());
}

void test_unsupported_length_resynchronizes()
{
    Fixture fixture;
    const ScorpionFrame frame = goldenFrame();
    std::vector<uint8_t> bytes = {0x55, 0x00, 0x21, 0x12, 0x34};
    bytes.insert(bytes.end(), frame.begin(), frame.end());

    SerialScorpionTlmTestAccess::process(fixture.serial, bytes.data(), bytes.size());

    TEST_ASSERT_EQUAL_UINT32(1, SerialScorpionTlmTestAccess::validFrameCount(fixture.serial));
}

void test_partial_frame_timeout_boundary()
{
    Fixture fixture;
    const ScorpionFrame frame = goldenFrame();

    SerialScorpionTlmTestAccess::process(fixture.serial, frame.data(), 10);
    TEST_ASSERT_EQUAL_UINT8(10, SerialScorpionTlmTestAccess::framePosition(fixture.serial));

    SerialScorpionTlmTestAccess::setLastReceivedByteMs(fixture.serial, static_cast<uint32_t>(millis()));
    fixture.serial.sendQueuedData(0);
    TEST_ASSERT_EQUAL_UINT8(10, SerialScorpionTlmTestAccess::framePosition(fixture.serial));

    SerialScorpionTlmTestAccess::setLastReceivedByteMs(
        fixture.serial, static_cast<uint32_t>(millis()) - 50U);
    fixture.serial.sendQueuedData(0);
    TEST_ASSERT_EQUAL_UINT8(0, SerialScorpionTlmTestAccess::framePosition(fixture.serial));

    SerialScorpionTlmTestAccess::process(fixture.serial, frame.data(), frame.size());
    TEST_ASSERT_EQUAL_UINT32(1, SerialScorpionTlmTestAccess::validFrameCount(fixture.serial));
}

void test_serialio_input_path_is_bounded_and_chunked()
{
    Fixture fixture;
    const ScorpionFrame frame = goldenFrame();

    fixture.input.input.assign(64, 0xAA);
    fixture.input.input.insert(fixture.input.input.end(), frame.begin(), frame.end());

    fixture.serial.processSerialInput();
    TEST_ASSERT_EQUAL_INT(frame.size(), fixture.input.available());
    TEST_ASSERT_EQUAL_UINT32(0, SerialScorpionTlmTestAccess::validFrameCount(fixture.serial));

    fixture.serial.processSerialInput();
    TEST_ASSERT_EQUAL_INT(0, fixture.input.available());
    TEST_ASSERT_EQUAL_UINT32(1, SerialScorpionTlmTestAccess::validFrameCount(fixture.serial));
}

void test_serialio_input_path_handles_back_to_back_frames()
{
    Fixture fixture;
    const ScorpionFrame frame = goldenFrame();

    fixture.input.input.insert(fixture.input.input.end(), frame.begin(), frame.end());
    fixture.input.input.insert(fixture.input.input.end(), frame.begin(), frame.end());
    fixture.serial.processSerialInput();

    TEST_ASSERT_EQUAL_UINT32(2, SerialScorpionTlmTestAccess::validFrameCount(fixture.serial));
    TEST_ASSERT_EQUAL_INT(0, fixture.input.available());
}

void test_serialio_continuous_noise_is_bounded()
{
    Fixture fixture;
    fixture.input.input.assign(256, 0xAA);

    fixture.serial.processSerialInput();

    TEST_ASSERT_EQUAL_INT(192, fixture.input.available());
    TEST_ASSERT_EQUAL_UINT32(0, SerialScorpionTlmTestAccess::validFrameCount(fixture.serial));
    TEST_ASSERT_EQUAL_UINT8(0, SerialScorpionTlmTestAccess::framePosition(fixture.serial));
}

void test_partial_frame_timeout_handles_millis_wrap()
{
    TEST_ASSERT_FALSE(SerialScorpionTlmTestAccess::partialFrameTimedOut(28, UINT32_MAX - 20));
    TEST_ASSERT_TRUE(SerialScorpionTlmTestAccess::partialFrameTimedOut(29, UINT32_MAX - 20));
}

void test_corrupt_frame_followed_by_valid_frame_resynchronizes()
{
    Fixture fixture;
    ScorpionFrame corrupt = goldenFrame();
    const ScorpionFrame valid = goldenFrame();
    corrupt[21] ^= 0x80;
    std::array<uint8_t, 44> frames;
    std::copy(corrupt.begin(), corrupt.end(), frames.begin());
    std::copy(valid.begin(), valid.end(), frames.begin() + corrupt.size());

    SerialScorpionTlmTestAccess::process(fixture.serial, frames.data(), frames.size());

    TEST_ASSERT_EQUAL_UINT32(1, SerialScorpionTlmTestAccess::validFrameCount(fixture.serial));
}

void test_embedded_sync_byte_does_not_lose_frame_alignment()
{
    Fixture fixture;
    const ScorpionFrame embedded = embeddedSyncFrame();
    const ScorpionFrame valid = goldenFrame();
    std::array<uint8_t, 44> frames;
    std::copy(embedded.begin(), embedded.end(), frames.begin());
    std::copy(valid.begin(), valid.end(), frames.begin() + embedded.size());

    SerialScorpionTlmTestAccess::process(fixture.serial, frames.data(), frames.size());

    TEST_ASSERT_EQUAL_UINT32(2, SerialScorpionTlmTestAccess::validFrameCount(fixture.serial));
    TEST_ASSERT_EQUAL_UINT16(500, SerialScorpionTlmTestAccess::throttlePermille(fixture.serial));
}

void test_maximum_values_do_not_overflow()
{
    Fixture fixture;
    const ScorpionFrame frame = maximumFrame();

    SerialScorpionTlmTestAccess::process(fixture.serial, frame.data(), frame.size());

    TEST_ASSERT_EQUAL_HEX32(0xFFFFFF, SerialScorpionTlmTestAccess::timestampMs(fixture.serial));
    TEST_ASSERT_EQUAL_UINT16(1275, SerialScorpionTlmTestAccess::throttlePermille(fixture.serial));
    TEST_ASSERT_EQUAL_UINT16(0xFFFF, SerialScorpionTlmTestAccess::currentDeciAmps(fixture.serial));
    TEST_ASSERT_EQUAL_UINT16(0xFFFF, SerialScorpionTlmTestAccess::voltageDeciVolts(fixture.serial));
    TEST_ASSERT_EQUAL_UINT16(0xFFFF, SerialScorpionTlmTestAccess::consumedMah(fixture.serial));
    TEST_ASSERT_EQUAL_UINT16(2550, SerialScorpionTlmTestAccess::temperatureDeciCelsius(fixture.serial));
    TEST_ASSERT_EQUAL_UINT16(1275, SerialScorpionTlmTestAccess::pwmPermille(fixture.serial));
    TEST_ASSERT_EQUAL_UINT16(25500, SerialScorpionTlmTestAccess::becMillivolts(fixture.serial));
    TEST_ASSERT_EQUAL_UINT32(327675, SerialScorpionTlmTestAccess::rpm(fixture.serial));
    TEST_ASSERT_EQUAL_HEX8(0xFF, SerialScorpionTlmTestAccess::status(fixture.serial));
}

void test_crsf_encoding_source_ids_and_schedule()
{
    Fixture fixture;
    const ScorpionFrame frame = goldenFrame();

    for (uint8_t i = 0; i < 10; ++i)
    {
        SerialScorpionTlmTestAccess::process(fixture.serial, frame.data(), frame.size());
    }

    TEST_ASSERT_EQUAL_UINT32(12, fixture.connector.frames.size());

    const uint8_t expectedBattery[] = {0xC8, 0x0A, 0x08, 0x01, 0xF4, 0x01, 0xF4, 0x00, 0x12, 0x34, 0x00, 0x12};
    const uint8_t expectedRpm[] = {0xC8, 0x06, 0x0C, 0x00, 0x01, 0x05, 0xAE, 0x7B};
    const uint8_t expectedTemperature[] = {0xC8, 0x05, 0x0D, 0x00, 0x02, 0xEE, 0xBB};
    const uint8_t expectedBecVoltage[] = {0xC8, 0x05, 0x0E, 0x81, 0x20, 0x08, 0xCB};
    const uint8_t expectedFrameTypes[] = {
        CRSF_FRAMETYPE_BATTERY_SENSOR,
        CRSF_FRAMETYPE_RPM,
        CRSF_FRAMETYPE_BATTERY_SENSOR,
        CRSF_FRAMETYPE_RPM,
        CRSF_FRAMETYPE_BATTERY_SENSOR,
        CRSF_FRAMETYPE_TEMP,
        CRSF_FRAMETYPE_RPM,
        CRSF_FRAMETYPE_BATTERY_SENSOR,
        CRSF_FRAMETYPE_RPM,
        CRSF_FRAMETYPE_BATTERY_SENSOR,
        CRSF_FRAMETYPE_RPM,
        CRSF_FRAMETYPE_CELLS
    };

    for (uint8_t i = 0; i < sizeof(expectedFrameTypes); ++i)
    {
        TEST_ASSERT_EQUAL_HEX8(expectedFrameTypes[i], fixture.connector.frames[i][2]);
    }

    TEST_ASSERT_EQUAL_UINT8_ARRAY(expectedBattery, fixture.connector.frames[0].data(), sizeof(expectedBattery));
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expectedRpm, fixture.connector.frames[1].data(), sizeof(expectedRpm));
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expectedBattery, fixture.connector.frames[4].data(), sizeof(expectedBattery));
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expectedTemperature, fixture.connector.frames[5].data(), sizeof(expectedTemperature));
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expectedRpm, fixture.connector.frames[10].data(), sizeof(expectedRpm));
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expectedBecVoltage, fixture.connector.frames[11].data(), sizeof(expectedBecVoltage));
    TEST_ASSERT_TRUE(fixture.output.output.empty());
}

void test_driver_does_not_write_to_esc()
{
    Fixture fixture;

    TEST_ASSERT_EQUAL_UINT32(DURATION_IMMEDIATELY, fixture.serial.sendRCFrame(false, false, nullptr));
    fixture.serial.sendQueuedData(128);

    TEST_ASSERT_TRUE(fixture.output.output.empty());
}

void setUp()
{
    crsfBatterySensorDetected = false;
}

void tearDown()
{
}

int main(int argc, char **argv)
{
    UNITY_BEGIN();
    RUN_TEST(test_valid_frame_decodes_all_fields);
    RUN_TEST(test_incorrect_crc_is_rejected);
    RUN_TEST(test_noise_before_sync_is_ignored);
    RUN_TEST(test_frame_can_be_split_across_calls);
    RUN_TEST(test_multiple_frames_in_one_call);
    RUN_TEST(test_unsupported_length_resynchronizes);
    RUN_TEST(test_partial_frame_timeout_boundary);
    RUN_TEST(test_serialio_input_path_is_bounded_and_chunked);
    RUN_TEST(test_serialio_input_path_handles_back_to_back_frames);
    RUN_TEST(test_serialio_continuous_noise_is_bounded);
    RUN_TEST(test_partial_frame_timeout_handles_millis_wrap);
    RUN_TEST(test_corrupt_frame_followed_by_valid_frame_resynchronizes);
    RUN_TEST(test_embedded_sync_byte_does_not_lose_frame_alignment);
    RUN_TEST(test_maximum_values_do_not_overflow);
    RUN_TEST(test_crsf_encoding_source_ids_and_schedule);
    RUN_TEST(test_driver_does_not_write_to_esc);
    return UNITY_END();
}

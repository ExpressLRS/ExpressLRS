#include <algorithm>
#include <array>
#include <cstdint>
#include <vector>

#include <unity.h>

#include "CRSFRouter.h"
#include "rx-serial/SerialHobbywing_TLM.h"
#include "common.h"

CRSFRouter crsfRouter;
bool crsfBatterySensorDetected = false;

// Receiver serial implementations live in the application source tree, which
// PlatformIO does not link into individual native test programs by default.
#include "../../src/rx-serial/SerialIO.cpp"
#include "../../src/rx-serial/SerialHobbywing_TLM.cpp"

static_assert(PROTOCOL_HOBBYWING_TLM == 11, "Unexpected Hobbywing protocol value");
static_assert(PROTOCOL_HOBBYWING_TLM < 16, "Hobbywing protocol does not fit persisted storage");

namespace
{
class MockSerial : public HardwareSerial
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

    void updateBaudRate(unsigned long value) override { baud = value; }

    std::vector<uint8_t> input;
    std::vector<uint8_t> output;
    unsigned long baud = 19200;

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

using HobbywingV4InfoFrame = std::array<uint8_t, 13>;
using HobbywingV4DataFrame = std::array<uint8_t, 19>;
using HobbywingV5Frame = std::array<uint8_t, 32>;

// Voltage scale 1/2 (0.05V per count), current scale 1/10 (0.1A per count), offset 10A.
HobbywingV4InfoFrame v4InfoFrame()
{
    return {{
        0x9B, 0x9B,
        0x03, 0xE8,
        0x01,
        0x01, 0x02,
        0x01, 0x0A, 0x0A,
        0x00, 0x00,
        0xB9
    }};
}

// Throttle 500, 12345 eRPM, 400 voltage counts (20.0V), 150 current counts (5.0A), 64.4C.
HobbywingV4DataFrame v4DataFrame()
{
    return {{
        0x9B,
        0x00, 0x00, 0x01,
        0x01, 0xF4,
        0x01, 0x2C,
        0x00, 0x30, 0x39,
        0x01, 0x90,
        0x00, 0x96,
        0x08, 0x00,
        0x04, 0x00
    }};
}

// Worked example frame from the official Hobbywing V5 protocol document:
// throttle 28%, 28570 eRPM, 11.3V, 0.0A, 29C, no BEC fields.
HobbywingV5Frame v5GoldenFrame()
{
    return {{
        0xFE, 0x01, 0x00, 0x03, 0x30, 0x5C, 0x17, 0x06,
        0x00, 0x1C, 0x1C, 0x01, 0x00, 0x29, 0x0B, 0x71,
        0x00, 0x00, 0x00, 0x1D, 0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x64, 0xE8
    }};
}

struct Fixture
{
    Fixture()
        : serial(port)
    {
        crsfRouter.addConnector(&connector);
    }

    ~Fixture() { crsfRouter.removeConnector(&connector); }

    MockSerial port;
    MockConnector connector;
    SerialHobbywing_TLM serial;
};
}

class SerialHobbywingTlmTestAccess
{
public:
    static void process(SerialHobbywing_TLM &serial, const uint8_t *data, size_t length)
    {
        serial.processBytes(const_cast<uint8_t *>(data), static_cast<uint16_t>(length));
    }

    static bool partialFrameTimedOut(uint32_t now, uint32_t lastReceived)
    {
        return SerialHobbywing_TLM::partialFrameTimedOut(now, lastReceived);
    }

    static uint16_t crc(const uint8_t *data, size_t length) { return SerialHobbywing_TLM::calculateCrc(data, length); }
    static uint8_t framePosition(const SerialHobbywing_TLM &serial) { return serial.framePosition; }
    static bool protocolConfirmed(const SerialHobbywing_TLM &serial) { return serial.protocolConfirmed; }
    static uint32_t validFrameCount(const SerialHobbywing_TLM &serial) { return serial.validFrameCount; }
    static uint8_t voltageNumerator(const SerialHobbywing_TLM &serial) { return serial.calibration.voltageNumerator; }
    static uint8_t voltageDenominator(const SerialHobbywing_TLM &serial) { return serial.calibration.voltageDenominator; }
    static uint8_t currentNumerator(const SerialHobbywing_TLM &serial) { return serial.calibration.currentNumerator; }
    static uint8_t currentDenominator(const SerialHobbywing_TLM &serial) { return serial.calibration.currentDenominator; }
    static uint8_t currentOffset(const SerialHobbywing_TLM &serial) { return serial.calibration.currentOffset; }
    static uint32_t rpm(const SerialHobbywing_TLM &serial) { return serial.decoded.rpm; }
    static uint16_t voltageDeciVolts(const SerialHobbywing_TLM &serial) { return serial.decoded.voltageDeciVolts; }
    static uint16_t currentDeciAmps(const SerialHobbywing_TLM &serial) { return serial.decoded.currentDeciAmps; }
    static int16_t temperatureDeciCelsius(const SerialHobbywing_TLM &serial) { return serial.decoded.temperatureDeciCelsius; }
    static uint16_t becMillivolts(const SerialHobbywing_TLM &serial) { return serial.decoded.becMillivolts; }
    static bool hasBattery(const SerialHobbywing_TLM &serial) { return serial.decoded.hasBattery; }
    static bool hasTemperature(const SerialHobbywing_TLM &serial) { return serial.decoded.hasTemperature; }
    static bool hasBecVoltage(const SerialHobbywing_TLM &serial) { return serial.decoded.hasBecVoltage; }
};

namespace
{
// The driver starts on V4. Letting one dwell period pass without a confirmed
// protocol moves it to the other baud rate.
void expireProbeWindow(Fixture &fixture)
{
    SerialHobbywingTlmTestAccess::process(fixture.serial, nullptr, 0);
    nativeClockMs() += 1500;
    SerialHobbywingTlmTestAccess::process(fixture.serial, nullptr, 0);
}

// Main loop passes without serial input, one every 100ms.
void idle(Fixture &fixture, uint32_t durationMs)
{
    for (uint32_t elapsed = 0; elapsed < durationMs; elapsed += 100)
    {
        nativeClockMs() += 100;
        SerialHobbywingTlmTestAccess::process(fixture.serial, nullptr, 0);
    }
}

// Recompute the CRC after mutating a V5 frame in a test.
void finalizeFrame(HobbywingV5Frame &frame)
{
    const uint16_t crc = SerialHobbywingTlmTestAccess::crc(frame.data(), 30);
    frame[30] = static_cast<uint8_t>(crc & 0xFF);
    frame[31] = static_cast<uint8_t>(crc >> 8);
}
}

void test_v4_info_frame_confirms_protocol_and_updates_calibration()
{
    Fixture fixture;
    const HobbywingV4InfoFrame frame = v4InfoFrame();

    SerialHobbywingTlmTestAccess::process(fixture.serial, frame.data(), frame.size());

    TEST_ASSERT_TRUE(SerialHobbywingTlmTestAccess::protocolConfirmed(fixture.serial));
    TEST_ASSERT_EQUAL_UINT8(1, SerialHobbywingTlmTestAccess::voltageNumerator(fixture.serial));
    TEST_ASSERT_EQUAL_UINT8(2, SerialHobbywingTlmTestAccess::voltageDenominator(fixture.serial));
    TEST_ASSERT_EQUAL_UINT8(1, SerialHobbywingTlmTestAccess::currentNumerator(fixture.serial));
    TEST_ASSERT_EQUAL_UINT8(10, SerialHobbywingTlmTestAccess::currentDenominator(fixture.serial));
    TEST_ASSERT_EQUAL_UINT8(10, SerialHobbywingTlmTestAccess::currentOffset(fixture.serial));
    TEST_ASSERT_EQUAL_UINT8(0, SerialHobbywingTlmTestAccess::framePosition(fixture.serial));
}

void test_v4_three_consecutive_data_frames_confirm_protocol()
{
    Fixture fixture;
    const HobbywingV4DataFrame frame = v4DataFrame();

    SerialHobbywingTlmTestAccess::process(fixture.serial, frame.data(), frame.size());
    SerialHobbywingTlmTestAccess::process(fixture.serial, frame.data(), frame.size());
    TEST_ASSERT_FALSE(SerialHobbywingTlmTestAccess::protocolConfirmed(fixture.serial));
    TEST_ASSERT_TRUE(fixture.connector.frames.empty());

    SerialHobbywingTlmTestAccess::process(fixture.serial, frame.data(), frame.size());
    TEST_ASSERT_TRUE(SerialHobbywingTlmTestAccess::protocolConfirmed(fixture.serial));
}

void test_v4_implausible_frame_resets_consecutive_count()
{
    Fixture fixture;
    const HobbywingV4DataFrame frame = v4DataFrame();
    HobbywingV4DataFrame implausible = v4DataFrame();
    implausible[11] = 0x20; // voltage ADC high byte out of range

    SerialHobbywingTlmTestAccess::process(fixture.serial, frame.data(), frame.size());
    SerialHobbywingTlmTestAccess::process(fixture.serial, frame.data(), frame.size());
    SerialHobbywingTlmTestAccess::process(fixture.serial, implausible.data(), implausible.size());
    SerialHobbywingTlmTestAccess::process(fixture.serial, frame.data(), frame.size());
    SerialHobbywingTlmTestAccess::process(fixture.serial, frame.data(), frame.size());
    TEST_ASSERT_FALSE(SerialHobbywingTlmTestAccess::protocolConfirmed(fixture.serial));

    SerialHobbywingTlmTestAccess::process(fixture.serial, frame.data(), frame.size());
    TEST_ASSERT_TRUE(SerialHobbywingTlmTestAccess::protocolConfirmed(fixture.serial));
}

void test_v4_frames_without_sync_byte_are_ignored()
{
    Fixture fixture;

    // An all-zero frame passes every plausibility limit, so only the sync byte rejects it.
    const HobbywingV4DataFrame zeros = {};
    for (uint8_t i = 0; i < 3; ++i)
    {
        SerialHobbywingTlmTestAccess::process(fixture.serial, zeros.data(), zeros.size());
    }

    TEST_ASSERT_FALSE(SerialHobbywingTlmTestAccess::protocolConfirmed(fixture.serial));
    TEST_ASSERT_EQUAL_UINT8(0, SerialHobbywingTlmTestAccess::framePosition(fixture.serial));
}

void test_v4_data_frames_without_calibration_only_publish_rpm()
{
    Fixture fixture;
    const HobbywingV4DataFrame frame = v4DataFrame();

    for (uint8_t i = 0; i < 3; ++i)
    {
        SerialHobbywingTlmTestAccess::process(fixture.serial, frame.data(), frame.size());
    }

    TEST_ASSERT_FALSE(SerialHobbywingTlmTestAccess::hasBattery(fixture.serial));
    TEST_ASSERT_FALSE(crsfBatterySensorDetected);
    TEST_ASSERT_EQUAL_UINT32(1, fixture.connector.frames.size());
    TEST_ASSERT_EQUAL_HEX8(CRSF_FRAMETYPE_RPM, fixture.connector.frames[0][2]);
}

void test_v4_data_frame_decodes_big_endian_fields()
{
    Fixture fixture;
    const HobbywingV4InfoFrame info = v4InfoFrame();
    const HobbywingV4DataFrame data = v4DataFrame();

    SerialHobbywingTlmTestAccess::process(fixture.serial, info.data(), info.size());
    SerialHobbywingTlmTestAccess::process(fixture.serial, data.data(), data.size());

    TEST_ASSERT_EQUAL_UINT32(12345, SerialHobbywingTlmTestAccess::rpm(fixture.serial));
    TEST_ASSERT_EQUAL_UINT16(200, SerialHobbywingTlmTestAccess::voltageDeciVolts(fixture.serial));
    TEST_ASSERT_EQUAL_UINT16(50, SerialHobbywingTlmTestAccess::currentDeciAmps(fixture.serial));
    TEST_ASSERT_EQUAL_INT16(644, SerialHobbywingTlmTestAccess::temperatureDeciCelsius(fixture.serial));
    TEST_ASSERT_TRUE(SerialHobbywingTlmTestAccess::hasBattery(fixture.serial));
    TEST_ASSERT_TRUE(crsfBatterySensorDetected);
}

void test_v4_zero_throttle_forces_current_to_zero()
{
    Fixture fixture;
    const HobbywingV4InfoFrame info = v4InfoFrame();
    HobbywingV4DataFrame data = v4DataFrame();
    data[4] = 0x00;
    data[5] = 0x00;

    SerialHobbywingTlmTestAccess::process(fixture.serial, info.data(), info.size());
    SerialHobbywingTlmTestAccess::process(fixture.serial, data.data(), data.size());

    TEST_ASSERT_EQUAL_UINT32(1, SerialHobbywingTlmTestAccess::validFrameCount(fixture.serial));
    TEST_ASSERT_EQUAL_UINT16(0, SerialHobbywingTlmTestAccess::currentDeciAmps(fixture.serial));
}

void test_v4_current_below_offset_clamps_to_zero()
{
    Fixture fixture;
    const HobbywingV4InfoFrame info = v4InfoFrame();
    HobbywingV4DataFrame data = v4DataFrame();
    data[13] = 0x00; // 80 counts -> 8.0A raw, below the 10A offset
    data[14] = 0x50;

    SerialHobbywingTlmTestAccess::process(fixture.serial, info.data(), info.size());
    SerialHobbywingTlmTestAccess::process(fixture.serial, data.data(), data.size());

    TEST_ASSERT_EQUAL_UINT32(1, SerialHobbywingTlmTestAccess::validFrameCount(fixture.serial));
    TEST_ASSERT_TRUE(SerialHobbywingTlmTestAccess::hasBattery(fixture.serial));
    TEST_ASSERT_EQUAL_UINT16(0, SerialHobbywingTlmTestAccess::currentDeciAmps(fixture.serial));
    TEST_ASSERT_EQUAL_UINT16(200, SerialHobbywingTlmTestAccess::voltageDeciVolts(fixture.serial));
}

void test_v4_high_voltage_is_scaled_and_encoded()
{
    Fixture fixture;
    HobbywingV4InfoFrame info = v4InfoFrame();
    HobbywingV4DataFrame data = v4DataFrame();
    info[5] = 0x02;
    info[6] = 0x0A; // 0.02V per ADC count
    data[11] = 0x0D;
    data[12] = 0xAC; // 3500 counts = 70V

    SerialHobbywingTlmTestAccess::process(fixture.serial, info.data(), info.size());
    SerialHobbywingTlmTestAccess::process(fixture.serial, data.data(), data.size());

    TEST_ASSERT_EQUAL_UINT16(700, SerialHobbywingTlmTestAccess::voltageDeciVolts(fixture.serial));
    TEST_ASSERT_EQUAL_UINT32(1, fixture.connector.frames.size());
    TEST_ASSERT_EQUAL_HEX8(0x02, fixture.connector.frames[0][3]);
    TEST_ASSERT_EQUAL_HEX8(0xBC, fixture.connector.frames[0][4]);
}

void test_v4_conversions_round_to_nearest()
{
    Fixture fixture;
    HobbywingV4InfoFrame info = v4InfoFrame();
    HobbywingV4DataFrame data = v4DataFrame();
    info[8] = 0x03;  // current scale 1/3
    data[12] = 0x91; // 401 counts -> 20.05V
    data[14] = 0x65; // 101 counts -> 33.67A raw, 23.67A after the 10A offset

    SerialHobbywingTlmTestAccess::process(fixture.serial, info.data(), info.size());
    SerialHobbywingTlmTestAccess::process(fixture.serial, data.data(), data.size());

    TEST_ASSERT_EQUAL_UINT16(201, SerialHobbywingTlmTestAccess::voltageDeciVolts(fixture.serial));
    TEST_ASSERT_EQUAL_UINT16(237, SerialHobbywingTlmTestAccess::currentDeciAmps(fixture.serial));
}

void test_v4_documented_info_frames_calibrate_battery()
{
    // INFO frames documented by the MSRC project for two ESC models, with the
    // values expected for 2000 voltage counts and 1000 current counts. The first
    // model reports no current scale and must still deliver the voltage.
    const struct
    {
        HobbywingV4InfoFrame info;
        uint16_t deciVolts;
        uint16_t deciAmps;
    } cases[] = {
        {{{0x9B, 0x9B, 0x03, 0xE8, 0x01, 0x08, 0x5B, 0x00, 0x01, 0x00, 0x21, 0x21, 0xB9}}, 176, 0},    // V4 LV 25/60/80A
        {{{0x9B, 0x9B, 0x03, 0xE8, 0x01, 0x02, 0x0D, 0x0A, 0x3D, 0x05, 0x1E, 0x21, 0xB9}}, 308, 1589}, // V4 HV 200A OPTO
    };

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i)
    {
        Fixture fixture;
        HobbywingV4DataFrame data = v4DataFrame();
        data[11] = 0x07;
        data[12] = 0xD0;
        data[13] = 0x03;
        data[14] = 0xE8;
        crsfBatterySensorDetected = false;

        SerialHobbywingTlmTestAccess::process(fixture.serial, cases[i].info.data(), cases[i].info.size());
        SerialHobbywingTlmTestAccess::process(fixture.serial, data.data(), data.size());

        TEST_ASSERT_TRUE(SerialHobbywingTlmTestAccess::hasBattery(fixture.serial));
        TEST_ASSERT_EQUAL_UINT16(cases[i].deciVolts, SerialHobbywingTlmTestAccess::voltageDeciVolts(fixture.serial));
        TEST_ASSERT_EQUAL_UINT16(cases[i].deciAmps, SerialHobbywingTlmTestAccess::currentDeciAmps(fixture.serial));
        TEST_ASSERT_TRUE(crsfBatterySensorDetected);
    }
}

void test_v4_zero_scale_keeps_previous_scale_and_full_info_recalibrates()
{
    Fixture fixture;
    const HobbywingV4InfoFrame info = v4InfoFrame();
    HobbywingV4InfoFrame partial = v4InfoFrame();
    partial[5] = 0x00; // zeroed voltage scale pair must keep the previous scale
    partial[6] = 0x00;
    HobbywingV4InfoFrame rescale = v4InfoFrame();
    rescale[5] = 0x02; // voltage scale 2/10: 400 counts -> 8.0V
    rescale[6] = 0x0A;
    const HobbywingV4DataFrame data = v4DataFrame();

    SerialHobbywingTlmTestAccess::process(fixture.serial, info.data(), info.size());
    SerialHobbywingTlmTestAccess::process(fixture.serial, partial.data(), partial.size());
    SerialHobbywingTlmTestAccess::process(fixture.serial, data.data(), data.size());

    TEST_ASSERT_EQUAL_UINT16(200, SerialHobbywingTlmTestAccess::voltageDeciVolts(fixture.serial));

    SerialHobbywingTlmTestAccess::process(fixture.serial, rescale.data(), rescale.size());
    SerialHobbywingTlmTestAccess::process(fixture.serial, data.data(), data.size());

    TEST_ASSERT_EQUAL_UINT16(80, SerialHobbywingTlmTestAccess::voltageDeciVolts(fixture.serial));
}

void test_v4_values_above_sanity_caps_are_rejected()
{
    Fixture fixture;
    HobbywingV4InfoFrame info = v4InfoFrame();
    info[7] = 0xFF; // current scale 255/1 makes ADC 0x0FFF decode to 1,044,215A
    info[8] = 0x01;
    HobbywingV4DataFrame overCurrent = v4DataFrame();
    overCurrent[13] = 0x0F;
    overCurrent[14] = 0xFF;
    HobbywingV4DataFrame overVoltage = v4DataFrame();
    overVoltage[11] = 0x0F; // 4000 counts -> 200V
    overVoltage[12] = 0xA0;
    overVoltage[13] = 0x00; // no current
    overVoltage[14] = 0x00;

    SerialHobbywingTlmTestAccess::process(fixture.serial, info.data(), info.size());
    SerialHobbywingTlmTestAccess::process(fixture.serial, overCurrent.data(), overCurrent.size());
    TEST_ASSERT_EQUAL_UINT32(0, SerialHobbywingTlmTestAccess::validFrameCount(fixture.serial));

    SerialHobbywingTlmTestAccess::process(fixture.serial, overVoltage.data(), overVoltage.size());
    TEST_ASSERT_EQUAL_UINT32(0, SerialHobbywingTlmTestAccess::validFrameCount(fixture.serial));
}

void test_v4_rpm_cap_matches_crsf_int24()
{
    Fixture fixture;
    const HobbywingV4InfoFrame info = v4InfoFrame();
    HobbywingV4DataFrame maxRpm = v4DataFrame();
    maxRpm[8] = 0x7F; // 8,388,607, the largest value the signed 24-bit CRSF field carries
    maxRpm[9] = 0xFF;
    maxRpm[10] = 0xFF;
    HobbywingV4DataFrame overRpm = v4DataFrame();
    overRpm[8] = 0x80; // 8,388,608 would encode as a negative RPM
    overRpm[9] = 0x00;
    overRpm[10] = 0x00;

    SerialHobbywingTlmTestAccess::process(fixture.serial, info.data(), info.size());
    SerialHobbywingTlmTestAccess::process(fixture.serial, maxRpm.data(), maxRpm.size());
    TEST_ASSERT_EQUAL_UINT32(1, SerialHobbywingTlmTestAccess::validFrameCount(fixture.serial));
    TEST_ASSERT_EQUAL_UINT32(8388607, SerialHobbywingTlmTestAccess::rpm(fixture.serial));

    SerialHobbywingTlmTestAccess::process(fixture.serial, overRpm.data(), overRpm.size());
    TEST_ASSERT_EQUAL_UINT32(1, SerialHobbywingTlmTestAccess::validFrameCount(fixture.serial));
}

void test_v4_temperature_lut_boundaries_and_out_of_range_skip()
{
    Fixture fixture;
    const HobbywingV4InfoFrame info = v4InfoFrame();
    const struct
    {
        uint16_t adc;
        bool accepted;
        int16_t deciCelsius;
    } cases[] = {
        {0, false, 0},      // 894.3 C
        {63, false, 0},     // 250.6 C, still inside the first table interval
        {64, true, 2495},   // first accepted reading, the second table entry
        {2048, true, 644},  // exact table entry
        {2080, true, 635},  // halfway between two entries
        {3872, true, -18},  // negative reading proves the int16_t cast
        {4032, true, -238}, // last accepted reading, the second to last table entry
        {4033, false, 0},   // -24.1 C, inside the last table interval
        {4095, false, 0},   // -75.8 C
    };

    SerialHobbywingTlmTestAccess::process(fixture.serial, info.data(), info.size());

    uint32_t published = 0;
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i)
    {
        HobbywingV4DataFrame data = v4DataFrame();
        data[15] = static_cast<uint8_t>(cases[i].adc >> 8);
        data[16] = static_cast<uint8_t>(cases[i].adc & 0xFF);

        SerialHobbywingTlmTestAccess::process(fixture.serial, data.data(), data.size());

        // An out-of-range temperature must not drop the rest of the frame.
        ++published;
        TEST_ASSERT_EQUAL_UINT32(published, SerialHobbywingTlmTestAccess::validFrameCount(fixture.serial));
        if (cases[i].accepted)
        {
            TEST_ASSERT_TRUE(SerialHobbywingTlmTestAccess::hasTemperature(fixture.serial));
            TEST_ASSERT_EQUAL_INT16(cases[i].deciCelsius, SerialHobbywingTlmTestAccess::temperatureDeciCelsius(fixture.serial));
        }
        else
        {
            TEST_ASSERT_FALSE(SerialHobbywingTlmTestAccess::hasTemperature(fixture.serial));
        }
    }
}

void test_v4_out_of_range_temperature_is_not_sent()
{
    Fixture fixture;
    const HobbywingV4InfoFrame info = v4InfoFrame();
    HobbywingV4DataFrame data = v4DataFrame();
    data[15] = 0x00; // railed NTC reading
    data[16] = 0x00;

    SerialHobbywingTlmTestAccess::process(fixture.serial, info.data(), info.size());
    for (uint8_t i = 0; i < 10; ++i)
    {
        SerialHobbywingTlmTestAccess::process(fixture.serial, data.data(), data.size());
    }

    // Ten frames: battery and RPM alternate and both slow slots stay empty.
    TEST_ASSERT_EQUAL_UINT32(10, fixture.connector.frames.size());
    for (size_t i = 0; i < fixture.connector.frames.size(); ++i)
    {
        const uint8_t expectedType = (i % 2 == 0) ? CRSF_FRAMETYPE_BATTERY_SENSOR : CRSF_FRAMETYPE_RPM;
        TEST_ASSERT_EQUAL_HEX8(expectedType, fixture.connector.frames[i][2]);
    }
}

void test_v4_optional_trailing_marker_is_ignored()
{
    Fixture fixture;
    const HobbywingV4InfoFrame info = v4InfoFrame();
    const HobbywingV4DataFrame data = v4DataFrame();
    std::array<uint8_t, 20> framed = {};
    std::copy(data.begin(), data.end(), framed.begin());
    framed[19] = 0xB9;

    SerialHobbywingTlmTestAccess::process(fixture.serial, info.data(), info.size());
    SerialHobbywingTlmTestAccess::process(fixture.serial, framed.data(), framed.size());
    SerialHobbywingTlmTestAccess::process(fixture.serial, data.data(), data.size());

    TEST_ASSERT_EQUAL_UINT32(2, SerialHobbywingTlmTestAccess::validFrameCount(fixture.serial));
}

void test_v4_frame_can_be_split_across_calls()
{
    Fixture fixture;
    const HobbywingV4InfoFrame info = v4InfoFrame();
    const HobbywingV4DataFrame data = v4DataFrame();

    SerialHobbywingTlmTestAccess::process(fixture.serial, info.data(), info.size());
    SerialHobbywingTlmTestAccess::process(fixture.serial, data.data(), 7);
    SerialHobbywingTlmTestAccess::process(fixture.serial, data.data() + 7, data.size() - 7);

    TEST_ASSERT_EQUAL_UINT32(1, SerialHobbywingTlmTestAccess::validFrameCount(fixture.serial));
    TEST_ASSERT_EQUAL_UINT32(12345, SerialHobbywingTlmTestAccess::rpm(fixture.serial));
}

void test_v4_partial_frame_timeout_boundary()
{
    Fixture fixture;
    const HobbywingV4InfoFrame info = v4InfoFrame();
    const HobbywingV4DataFrame data = v4DataFrame();

    SerialHobbywingTlmTestAccess::process(fixture.serial, info.data(), info.size());
    SerialHobbywingTlmTestAccess::process(fixture.serial, data.data(), 7);
    TEST_ASSERT_EQUAL_UINT8(7, SerialHobbywingTlmTestAccess::framePosition(fixture.serial));

    nativeClockMs() += 49;
    SerialHobbywingTlmTestAccess::process(fixture.serial, nullptr, 0);
    TEST_ASSERT_EQUAL_UINT8(7, SerialHobbywingTlmTestAccess::framePosition(fixture.serial));

    nativeClockMs() += 1;
    SerialHobbywingTlmTestAccess::process(fixture.serial, nullptr, 0);
    TEST_ASSERT_EQUAL_UINT8(0, SerialHobbywingTlmTestAccess::framePosition(fixture.serial));

    SerialHobbywingTlmTestAccess::process(fixture.serial, data.data(), data.size());
    TEST_ASSERT_EQUAL_UINT32(1, SerialHobbywingTlmTestAccess::validFrameCount(fixture.serial));
}

void test_partial_frame_timeout_handles_millis_wrap()
{
    TEST_ASSERT_FALSE(SerialHobbywingTlmTestAccess::partialFrameTimedOut(28, UINT32_MAX - 20));
    TEST_ASSERT_TRUE(SerialHobbywingTlmTestAccess::partialFrameTimedOut(29, UINT32_MAX - 20));
}

void test_v4_serial_input_path_handles_back_to_back_frames()
{
    Fixture fixture;
    const HobbywingV4InfoFrame info = v4InfoFrame();
    const HobbywingV4DataFrame data = v4DataFrame();

    fixture.port.input.insert(fixture.port.input.end(), info.begin(), info.end());
    fixture.port.input.insert(fixture.port.input.end(), data.begin(), data.end());
    fixture.port.input.insert(fixture.port.input.end(), data.begin(), data.end());
    fixture.serial.processSerialInput();

    TEST_ASSERT_EQUAL_UINT32(2, SerialHobbywingTlmTestAccess::validFrameCount(fixture.serial));
    TEST_ASSERT_EQUAL_INT(0, fixture.port.available());
}

void test_v4_noise_and_malformed_input_are_recovered_from()
{
    Fixture fixture;
    HobbywingV4DataFrame malformed = v4DataFrame();
    const HobbywingV4InfoFrame info = v4InfoFrame();
    const HobbywingV4DataFrame valid = v4DataFrame();
    malformed[11] = 0x20;

    const uint8_t noise[] = {0x00, 0x7E, 0xB9, 0x12};
    SerialHobbywingTlmTestAccess::process(fixture.serial, noise, sizeof(noise));
    SerialHobbywingTlmTestAccess::process(fixture.serial, malformed.data(), malformed.size() - 1);
    SerialHobbywingTlmTestAccess::process(fixture.serial, info.data(), info.size());
    SerialHobbywingTlmTestAccess::process(fixture.serial, valid.data(), valid.size());

    TEST_ASSERT_EQUAL_UINT32(1, SerialHobbywingTlmTestAccess::validFrameCount(fixture.serial));
    TEST_ASSERT_TRUE(SerialHobbywingTlmTestAccess::hasBattery(fixture.serial));
}

void test_v4_serial_input_continuous_sync_flood_is_bounded()
{
    Fixture fixture;
    fixture.port.input.assign(256, 0x9B);

    fixture.serial.processSerialInput();

    TEST_ASSERT_EQUAL_INT(192, fixture.port.available());
    TEST_ASSERT_TRUE(SerialHobbywingTlmTestAccess::framePosition(fixture.serial) <= 18);

    while (fixture.port.available() > 0)
    {
        fixture.serial.processSerialInput();
    }
    TEST_ASSERT_FALSE(SerialHobbywingTlmTestAccess::protocolConfirmed(fixture.serial));
    TEST_ASSERT_TRUE(fixture.connector.frames.empty());
}

void test_v4_embedded_sync_byte_keeps_frame_alignment()
{
    Fixture fixture;
    const HobbywingV4InfoFrame info = v4InfoFrame();
    const HobbywingV4DataFrame first = v4DataFrame();
    HobbywingV4DataFrame second = v4DataFrame();
    second[13] = 0x00; // 155 counts -> 5.5A, with an embedded 0x9B payload byte
    second[14] = 0x9B;

    SerialHobbywingTlmTestAccess::process(fixture.serial, info.data(), info.size());
    SerialHobbywingTlmTestAccess::process(fixture.serial, first.data(), first.size());
    SerialHobbywingTlmTestAccess::process(fixture.serial, second.data(), second.size());

    TEST_ASSERT_EQUAL_UINT32(2, SerialHobbywingTlmTestAccess::validFrameCount(fixture.serial));
    TEST_ASSERT_EQUAL_UINT16(55, SerialHobbywingTlmTestAccess::currentDeciAmps(fixture.serial));
}

void test_v4_invalid_info_frames_are_rejected()
{
    const struct
    {
        uint8_t index;
        uint8_t value;
    } cases[] = {
        {1, 0x00},  // second sync byte
        {4, 0x02},  // RPM steps
        {12, 0x00}, // trailing marker
    };

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i)
    {
        Fixture fixture;
        HobbywingV4InfoFrame info = v4InfoFrame();
        info[cases[i].index] = cases[i].value;

        SerialHobbywingTlmTestAccess::process(fixture.serial, info.data(), info.size());

        TEST_ASSERT_FALSE(SerialHobbywingTlmTestAccess::protocolConfirmed(fixture.serial));
        TEST_ASSERT_EQUAL_UINT8(0, SerialHobbywingTlmTestAccess::voltageDenominator(fixture.serial));
    }
}

void test_v4_implausible_data_frames_are_rejected()
{
    // High bytes of throttle, PWM and the four ADC readings.
    const uint8_t highBytes[] = {4, 6, 11, 13, 15, 17};

    for (size_t i = 0; i < sizeof(highBytes); ++i)
    {
        Fixture fixture;
        HobbywingV4DataFrame data = v4DataFrame();
        data[highBytes[i]] = 0x20;

        // Three valid frames would confirm the protocol.
        for (uint8_t repeat = 0; repeat < 3; ++repeat)
        {
            SerialHobbywingTlmTestAccess::process(fixture.serial, data.data(), data.size());
        }

        TEST_ASSERT_FALSE(SerialHobbywingTlmTestAccess::protocolConfirmed(fixture.serial));
        TEST_ASSERT_TRUE(fixture.connector.frames.empty());
    }
}

void test_v4_crsf_encoding_and_telemetry_schedule()
{
    Fixture fixture;
    const HobbywingV4InfoFrame info = v4InfoFrame();
    const HobbywingV4DataFrame data = v4DataFrame();

    SerialHobbywingTlmTestAccess::process(fixture.serial, info.data(), info.size());
    for (uint8_t i = 0; i < 10; ++i)
    {
        SerialHobbywingTlmTestAccess::process(fixture.serial, data.data(), data.size());
    }

    // Ten frames: battery/RPM alternate, the 5th and the 10th add temperature.
    TEST_ASSERT_TRUE(crsfBatterySensorDetected);
    TEST_ASSERT_EQUAL_UINT32(12, fixture.connector.frames.size());

    const uint8_t expectedBattery[] = {0xC8, 0x0A, 0x08, 0x00, 0xC8, 0x00, 0x32, 0x00, 0x00, 0x00, 0x00, 0x3E};
    const uint8_t expectedRpm[] = {0xC8, 0x06, 0x0C, 0x00, 0x00, 0x30, 0x39, 0x33};
    const uint8_t expectedTemperature[] = {0xC8, 0x05, 0x0D, 0x00, 0x02, 0x84, 0xD4};

    TEST_ASSERT_EQUAL_UINT8_ARRAY(expectedBattery, fixture.connector.frames[0].data(), sizeof(expectedBattery));
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expectedRpm, fixture.connector.frames[1].data(), sizeof(expectedRpm));
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expectedBattery, fixture.connector.frames[4].data(), sizeof(expectedBattery));
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expectedTemperature, fixture.connector.frames[5].data(), sizeof(expectedTemperature));
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expectedRpm, fixture.connector.frames[10].data(), sizeof(expectedRpm));
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expectedTemperature, fixture.connector.frames[11].data(), sizeof(expectedTemperature));
}

void test_first_probe_window_starts_at_the_first_call()
{
    Fixture fixture;

    // The receiver has been running for a while before the driver is first called.
    nativeClockMs() = 5000;
    SerialHobbywingTlmTestAccess::process(fixture.serial, nullptr, 0);
    TEST_ASSERT_EQUAL_UINT32(19200, fixture.port.baud);

    nativeClockMs() += 1499;
    SerialHobbywingTlmTestAccess::process(fixture.serial, nullptr, 0);
    TEST_ASSERT_EQUAL_UINT32(19200, fixture.port.baud);

    nativeClockMs() += 1;
    SerialHobbywingTlmTestAccess::process(fixture.serial, nullptr, 0);
    TEST_ASSERT_EQUAL_UINT32(115200, fixture.port.baud);
}

void test_silence_alternates_baud_rate()
{
    Fixture fixture;

    expireProbeWindow(fixture);
    TEST_ASSERT_EQUAL_UINT32(115200, fixture.port.baud);

    expireProbeWindow(fixture);
    TEST_ASSERT_EQUAL_UINT32(19200, fixture.port.baud);
    TEST_ASSERT_FALSE(SerialHobbywingTlmTestAccess::protocolConfirmed(fixture.serial));
}

void test_baud_switch_discards_pending_bytes_and_partial_frame()
{
    Fixture fixture;
    const HobbywingV5Frame frame = v5GoldenFrame();
    expireProbeWindow(fixture);
    fixture.port.input.assign(100, 0x55);

    // The dwell expires in the same call that delivers most of a V5 frame.
    nativeClockMs() += 1500;
    SerialHobbywingTlmTestAccess::process(fixture.serial, frame.data(), 25);

    TEST_ASSERT_EQUAL_UINT32(19200, fixture.port.baud);
    TEST_ASSERT_EQUAL_INT(0, fixture.port.available());
    TEST_ASSERT_EQUAL_UINT8(0, SerialHobbywingTlmTestAccess::framePosition(fixture.serial));
}

void test_baud_switch_restarts_frame_count()
{
    Fixture fixture;
    const HobbywingV5Frame frame = v5GoldenFrame();
    expireProbeWindow(fixture);

    // One valid frame, then silence until the driver has been to V4 and back.
    SerialHobbywingTlmTestAccess::process(fixture.serial, frame.data(), frame.size());
    expireProbeWindow(fixture);
    expireProbeWindow(fixture);
    TEST_ASSERT_EQUAL_UINT32(115200, fixture.port.baud);

    SerialHobbywingTlmTestAccess::process(fixture.serial, frame.data(), frame.size());
    TEST_ASSERT_FALSE(SerialHobbywingTlmTestAccess::protocolConfirmed(fixture.serial));

    SerialHobbywingTlmTestAccess::process(fixture.serial, frame.data(), frame.size());
    TEST_ASSERT_TRUE(SerialHobbywingTlmTestAccess::protocolConfirmed(fixture.serial));
}

void test_slow_frame_rate_still_confirms_protocol()
{
    // One frame per second does not fit the frames needed into one dwell period,
    // but each valid frame restarts the dwell.
    Fixture v4;
    const HobbywingV4DataFrame data = v4DataFrame();
    for (uint8_t i = 0; i < 3; ++i)
    {
        SerialHobbywingTlmTestAccess::process(v4.serial, data.data(), data.size());
        idle(v4, 1000);
    }
    TEST_ASSERT_TRUE(SerialHobbywingTlmTestAccess::protocolConfirmed(v4.serial));
    TEST_ASSERT_EQUAL_UINT32(19200, v4.port.baud);

    Fixture v5;
    const HobbywingV5Frame frame = v5GoldenFrame();
    expireProbeWindow(v5);
    idle(v5, 800);
    SerialHobbywingTlmTestAccess::process(v5.serial, frame.data(), frame.size());
    idle(v5, 1000);
    SerialHobbywingTlmTestAccess::process(v5.serial, frame.data(), frame.size());
    TEST_ASSERT_TRUE(SerialHobbywingTlmTestAccess::protocolConfirmed(v5.serial));
    TEST_ASSERT_EQUAL_UINT32(115200, v5.port.baud);
}

void test_confirmed_protocol_keeps_baud_rate()
{
    Fixture v4;
    const HobbywingV4InfoFrame info = v4InfoFrame();
    SerialHobbywingTlmTestAccess::process(v4.serial, info.data(), info.size());
    expireProbeWindow(v4);
    TEST_ASSERT_EQUAL_UINT32(19200, v4.port.baud);

    Fixture v5;
    const HobbywingV5Frame frame = v5GoldenFrame();
    expireProbeWindow(v5);
    SerialHobbywingTlmTestAccess::process(v5.serial, frame.data(), frame.size());
    SerialHobbywingTlmTestAccess::process(v5.serial, frame.data(), frame.size());
    expireProbeWindow(v5);
    TEST_ASSERT_EQUAL_UINT32(115200, v5.port.baud);
}

void test_v5_two_frames_confirm_protocol()
{
    Fixture fixture;
    const HobbywingV5Frame frame = v5GoldenFrame();
    expireProbeWindow(fixture);

    SerialHobbywingTlmTestAccess::process(fixture.serial, frame.data(), frame.size());
    TEST_ASSERT_FALSE(SerialHobbywingTlmTestAccess::protocolConfirmed(fixture.serial));
    TEST_ASSERT_TRUE(fixture.connector.frames.empty());

    SerialHobbywingTlmTestAccess::process(fixture.serial, frame.data(), frame.size());
    TEST_ASSERT_TRUE(SerialHobbywingTlmTestAccess::protocolConfirmed(fixture.serial));
    TEST_ASSERT_EQUAL_UINT32(1, fixture.connector.frames.size());
    TEST_ASSERT_EQUAL_HEX8(CRSF_FRAMETYPE_BATTERY_SENSOR, fixture.connector.frames[0][2]);
}

void test_v5_golden_frame_decodes_little_endian_fields()
{
    Fixture fixture;
    const HobbywingV5Frame frame = v5GoldenFrame();
    expireProbeWindow(fixture);

    SerialHobbywingTlmTestAccess::process(fixture.serial, frame.data(), frame.size());
    SerialHobbywingTlmTestAccess::process(fixture.serial, frame.data(), frame.size());

    TEST_ASSERT_EQUAL_UINT32(1, SerialHobbywingTlmTestAccess::validFrameCount(fixture.serial));
    TEST_ASSERT_EQUAL_UINT32(28570, SerialHobbywingTlmTestAccess::rpm(fixture.serial));
    TEST_ASSERT_EQUAL_UINT16(113, SerialHobbywingTlmTestAccess::voltageDeciVolts(fixture.serial));
    TEST_ASSERT_EQUAL_UINT16(0, SerialHobbywingTlmTestAccess::currentDeciAmps(fixture.serial));
    TEST_ASSERT_EQUAL_INT16(290, SerialHobbywingTlmTestAccess::temperatureDeciCelsius(fixture.serial));
    TEST_ASSERT_FALSE(SerialHobbywingTlmTestAccess::hasBecVoltage(fixture.serial));
    TEST_ASSERT_TRUE(crsfBatterySensorDetected);
}

void test_v5_incorrect_crc_is_rejected()
{
    Fixture fixture;
    HobbywingV5Frame frame = v5GoldenFrame();
    frame[15] ^= 0x01;
    expireProbeWindow(fixture);

    SerialHobbywingTlmTestAccess::process(fixture.serial, frame.data(), frame.size());
    SerialHobbywingTlmTestAccess::process(fixture.serial, frame.data(), frame.size());

    TEST_ASSERT_FALSE(SerialHobbywingTlmTestAccess::protocolConfirmed(fixture.serial));
    TEST_ASSERT_TRUE(fixture.connector.frames.empty());
}

void test_v5_wrong_version_and_frame_type_are_rejected()
{
    Fixture fixture;
    HobbywingV5Frame wrongVersion = v5GoldenFrame();
    wrongVersion[3] = 0x02;
    finalizeFrame(wrongVersion);
    HobbywingV5Frame wrongType = v5GoldenFrame();
    wrongType[4] = 0x31;
    finalizeFrame(wrongType);
    HobbywingV5Frame wrongTypeHigh = v5GoldenFrame();
    wrongTypeHigh[5] = 0x5D;
    finalizeFrame(wrongTypeHigh);
    expireProbeWindow(fixture);

    // Two valid frames would confirm the protocol, so each frame is sent twice.
    SerialHobbywingTlmTestAccess::process(fixture.serial, wrongVersion.data(), wrongVersion.size());
    SerialHobbywingTlmTestAccess::process(fixture.serial, wrongVersion.data(), wrongVersion.size());
    TEST_ASSERT_FALSE(SerialHobbywingTlmTestAccess::protocolConfirmed(fixture.serial));

    SerialHobbywingTlmTestAccess::process(fixture.serial, wrongType.data(), wrongType.size());
    SerialHobbywingTlmTestAccess::process(fixture.serial, wrongType.data(), wrongType.size());
    TEST_ASSERT_FALSE(SerialHobbywingTlmTestAccess::protocolConfirmed(fixture.serial));

    SerialHobbywingTlmTestAccess::process(fixture.serial, wrongTypeHigh.data(), wrongTypeHigh.size());
    SerialHobbywingTlmTestAccess::process(fixture.serial, wrongTypeHigh.data(), wrongTypeHigh.size());
    TEST_ASSERT_FALSE(SerialHobbywingTlmTestAccess::protocolConfirmed(fixture.serial));
}

void test_v5_noise_before_sync_is_ignored_and_frame_can_split()
{
    Fixture fixture;
    const HobbywingV5Frame frame = v5GoldenFrame();
    const uint8_t noise[] = {0x00, 0x7E, 0xB9, 0x12};
    expireProbeWindow(fixture);

    SerialHobbywingTlmTestAccess::process(fixture.serial, frame.data(), frame.size());
    SerialHobbywingTlmTestAccess::process(fixture.serial, noise, sizeof(noise));
    SerialHobbywingTlmTestAccess::process(fixture.serial, frame.data(), 10);
    SerialHobbywingTlmTestAccess::process(fixture.serial, frame.data() + 10, frame.size() - 10);

    TEST_ASSERT_EQUAL_UINT32(1, SerialHobbywingTlmTestAccess::validFrameCount(fixture.serial));
}

void test_v5_serial_input_path_is_chunked()
{
    Fixture fixture;
    const HobbywingV5Frame frame = v5GoldenFrame();
    expireProbeWindow(fixture);

    fixture.port.input.insert(fixture.port.input.end(), frame.begin(), frame.end());
    fixture.port.input.insert(fixture.port.input.end(), frame.begin(), frame.end());
    fixture.port.input.insert(fixture.port.input.end(), frame.begin(), frame.end());
    fixture.serial.processSerialInput();
    TEST_ASSERT_EQUAL_INT(32, fixture.port.available());

    fixture.serial.processSerialInput();
    TEST_ASSERT_EQUAL_INT(0, fixture.port.available());
    TEST_ASSERT_EQUAL_UINT32(2, SerialHobbywingTlmTestAccess::validFrameCount(fixture.serial));
}

void test_v5_corrupt_frame_restarts_consecutive_count()
{
    Fixture fixture;
    HobbywingV5Frame corrupt = v5GoldenFrame();
    corrupt[30] ^= 0x80;
    const HobbywingV5Frame valid = v5GoldenFrame();
    expireProbeWindow(fixture);

    SerialHobbywingTlmTestAccess::process(fixture.serial, valid.data(), valid.size());
    SerialHobbywingTlmTestAccess::process(fixture.serial, corrupt.data(), corrupt.size());
    SerialHobbywingTlmTestAccess::process(fixture.serial, valid.data(), valid.size());
    TEST_ASSERT_FALSE(SerialHobbywingTlmTestAccess::protocolConfirmed(fixture.serial));

    SerialHobbywingTlmTestAccess::process(fixture.serial, valid.data(), valid.size());
    TEST_ASSERT_TRUE(SerialHobbywingTlmTestAccess::protocolConfirmed(fixture.serial));
    TEST_ASSERT_EQUAL_UINT32(1, SerialHobbywingTlmTestAccess::validFrameCount(fixture.serial));
}

void test_v5_misaligned_stream_recovers_by_resynchronizing()
{
    Fixture fixture;
    expireProbeWindow(fixture);

    // A payload byte equal to the sync byte starts a misaligned candidate. The
    // suffix rescan must recover alignment on the following frames.
    HobbywingV5Frame frame = v5GoldenFrame();
    frame[17] = 0xFE; // current 0x01FE, low byte equals the sync byte
    frame[18] = 0x01;
    finalizeFrame(frame);

    SerialHobbywingTlmTestAccess::process(fixture.serial, frame.data() + 17, frame.size() - 17);
    SerialHobbywingTlmTestAccess::process(fixture.serial, frame.data(), frame.size());
    SerialHobbywingTlmTestAccess::process(fixture.serial, frame.data(), frame.size());

    TEST_ASSERT_TRUE(SerialHobbywingTlmTestAccess::protocolConfirmed(fixture.serial));
    TEST_ASSERT_EQUAL_UINT16(510, SerialHobbywingTlmTestAccess::currentDeciAmps(fixture.serial));
}

void test_v5_partial_frame_times_out()
{
    Fixture fixture;
    const HobbywingV5Frame frame = v5GoldenFrame();
    expireProbeWindow(fixture);

    SerialHobbywingTlmTestAccess::process(fixture.serial, frame.data(), 10);
    TEST_ASSERT_EQUAL_UINT8(10, SerialHobbywingTlmTestAccess::framePosition(fixture.serial));

    nativeClockMs() += 50;
    SerialHobbywingTlmTestAccess::process(fixture.serial, nullptr, 0);
    TEST_ASSERT_EQUAL_UINT8(0, SerialHobbywingTlmTestAccess::framePosition(fixture.serial));
}

void test_v5_zero_throttle_forces_current_to_zero()
{
    Fixture fixture;
    HobbywingV5Frame running = v5GoldenFrame();
    running[17] = 0x64; // 10.0A at 28% throttle
    finalizeFrame(running);
    HobbywingV5Frame stopped = running;
    stopped[9] = 0x00; // zero throttle, 10.0A still reported while spooling down
    finalizeFrame(stopped);
    expireProbeWindow(fixture);

    SerialHobbywingTlmTestAccess::process(fixture.serial, running.data(), running.size());
    SerialHobbywingTlmTestAccess::process(fixture.serial, running.data(), running.size());
    TEST_ASSERT_EQUAL_UINT16(100, SerialHobbywingTlmTestAccess::currentDeciAmps(fixture.serial));

    SerialHobbywingTlmTestAccess::process(fixture.serial, stopped.data(), stopped.size());
    TEST_ASSERT_EQUAL_UINT32(2, SerialHobbywingTlmTestAccess::validFrameCount(fixture.serial));
    TEST_ASSERT_EQUAL_UINT16(0, SerialHobbywingTlmTestAccess::currentDeciAmps(fixture.serial));
}

void test_v5_absent_bec_voltage_is_not_misread_as_255()
{
    Fixture fixture;
    const HobbywingV5Frame absent = v5GoldenFrame();
    HobbywingV5Frame populated = v5GoldenFrame();
    populated[22] = 0x32; // BEC voltage 5.0V
    finalizeFrame(populated);
    expireProbeWindow(fixture);

    SerialHobbywingTlmTestAccess::process(fixture.serial, populated.data(), populated.size());
    TEST_ASSERT_TRUE(SerialHobbywingTlmTestAccess::hasBecVoltage(fixture.serial));
    TEST_ASSERT_EQUAL_UINT16(5000, SerialHobbywingTlmTestAccess::becMillivolts(fixture.serial));

    SerialHobbywingTlmTestAccess::process(fixture.serial, absent.data(), absent.size());
    TEST_ASSERT_FALSE(SerialHobbywingTlmTestAccess::hasBecVoltage(fixture.serial));
    TEST_ASSERT_EQUAL_UINT16(0, SerialHobbywingTlmTestAccess::becMillivolts(fixture.serial));
}

void test_v5_crsf_encoding_and_telemetry_schedule()
{
    Fixture fixture;
    HobbywingV5Frame frame = v5GoldenFrame();
    frame[22] = 0x32; // BEC voltage 5.0V
    finalizeFrame(frame);
    expireProbeWindow(fixture);

    // The first frame only counts towards confirming the protocol.
    for (uint8_t i = 0; i < 11; ++i)
    {
        SerialHobbywingTlmTestAccess::process(fixture.serial, frame.data(), frame.size());
    }

    // Ten frames: battery/RPM alternate, the 5th adds temperature and the 10th
    // adds BEC voltage.
    TEST_ASSERT_EQUAL_UINT32(12, fixture.connector.frames.size());

    const uint8_t expectedBattery[] = {0xC8, 0x0A, 0x08, 0x00, 0x71, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x67};
    const uint8_t expectedRpm[] = {0xC8, 0x06, 0x0C, 0x00, 0x00, 0x6F, 0x9A, 0xB4};
    const uint8_t expectedTemperature[] = {0xC8, 0x05, 0x0D, 0x00, 0x01, 0x22, 0x03};
    const uint8_t expectedBecVoltage[] = {0xC8, 0x05, 0x0E, 0x81, 0x13, 0x88, 0x3C};

    TEST_ASSERT_EQUAL_UINT8_ARRAY(expectedBattery, fixture.connector.frames[0].data(), sizeof(expectedBattery));
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expectedRpm, fixture.connector.frames[1].data(), sizeof(expectedRpm));
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expectedTemperature, fixture.connector.frames[5].data(), sizeof(expectedTemperature));
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expectedBecVoltage, fixture.connector.frames[11].data(), sizeof(expectedBecVoltage));
}

void test_driver_does_not_write_to_esc()
{
    Fixture fixture;
    const HobbywingV4InfoFrame info = v4InfoFrame();
    const HobbywingV4DataFrame data = v4DataFrame();

    SerialHobbywingTlmTestAccess::process(fixture.serial, info.data(), info.size());
    SerialHobbywingTlmTestAccess::process(fixture.serial, data.data(), data.size());
    TEST_ASSERT_EQUAL_UINT32(DURATION_IMMEDIATELY, fixture.serial.sendRCFrame(false, false, nullptr));
    fixture.serial.sendQueuedData(128);

    TEST_ASSERT_TRUE(fixture.port.output.empty());
}

void setUp()
{
    crsfBatterySensorDetected = false;
    // The driver reads a stored time of 0 as not started yet, so start later.
    nativeClockMs() = 1000;
}

void tearDown()
{
}

int main(int argc, char **argv)
{
    UNITY_BEGIN();
    RUN_TEST(test_v4_info_frame_confirms_protocol_and_updates_calibration);
    RUN_TEST(test_v4_three_consecutive_data_frames_confirm_protocol);
    RUN_TEST(test_v4_implausible_frame_resets_consecutive_count);
    RUN_TEST(test_v4_frames_without_sync_byte_are_ignored);
    RUN_TEST(test_v4_data_frames_without_calibration_only_publish_rpm);
    RUN_TEST(test_v4_data_frame_decodes_big_endian_fields);
    RUN_TEST(test_v4_zero_throttle_forces_current_to_zero);
    RUN_TEST(test_v4_current_below_offset_clamps_to_zero);
    RUN_TEST(test_v4_high_voltage_is_scaled_and_encoded);
    RUN_TEST(test_v4_conversions_round_to_nearest);
    RUN_TEST(test_v4_documented_info_frames_calibrate_battery);
    RUN_TEST(test_v4_zero_scale_keeps_previous_scale_and_full_info_recalibrates);
    RUN_TEST(test_v4_values_above_sanity_caps_are_rejected);
    RUN_TEST(test_v4_rpm_cap_matches_crsf_int24);
    RUN_TEST(test_v4_temperature_lut_boundaries_and_out_of_range_skip);
    RUN_TEST(test_v4_out_of_range_temperature_is_not_sent);
    RUN_TEST(test_v4_optional_trailing_marker_is_ignored);
    RUN_TEST(test_v4_frame_can_be_split_across_calls);
    RUN_TEST(test_v4_partial_frame_timeout_boundary);
    RUN_TEST(test_partial_frame_timeout_handles_millis_wrap);
    RUN_TEST(test_v4_serial_input_path_handles_back_to_back_frames);
    RUN_TEST(test_v4_noise_and_malformed_input_are_recovered_from);
    RUN_TEST(test_v4_serial_input_continuous_sync_flood_is_bounded);
    RUN_TEST(test_v4_embedded_sync_byte_keeps_frame_alignment);
    RUN_TEST(test_v4_invalid_info_frames_are_rejected);
    RUN_TEST(test_v4_implausible_data_frames_are_rejected);
    RUN_TEST(test_v4_crsf_encoding_and_telemetry_schedule);
    RUN_TEST(test_first_probe_window_starts_at_the_first_call);
    RUN_TEST(test_silence_alternates_baud_rate);
    RUN_TEST(test_baud_switch_discards_pending_bytes_and_partial_frame);
    RUN_TEST(test_baud_switch_restarts_frame_count);
    RUN_TEST(test_slow_frame_rate_still_confirms_protocol);
    RUN_TEST(test_confirmed_protocol_keeps_baud_rate);
    RUN_TEST(test_v5_two_frames_confirm_protocol);
    RUN_TEST(test_v5_golden_frame_decodes_little_endian_fields);
    RUN_TEST(test_v5_incorrect_crc_is_rejected);
    RUN_TEST(test_v5_wrong_version_and_frame_type_are_rejected);
    RUN_TEST(test_v5_noise_before_sync_is_ignored_and_frame_can_split);
    RUN_TEST(test_v5_serial_input_path_is_chunked);
    RUN_TEST(test_v5_corrupt_frame_restarts_consecutive_count);
    RUN_TEST(test_v5_misaligned_stream_recovers_by_resynchronizing);
    RUN_TEST(test_v5_partial_frame_times_out);
    RUN_TEST(test_v5_zero_throttle_forces_current_to_zero);
    RUN_TEST(test_v5_absent_bec_voltage_is_not_misread_as_255);
    RUN_TEST(test_v5_crsf_encoding_and_telemetry_schedule);
    RUN_TEST(test_driver_does_not_write_to_esc);
    return UNITY_END();
}

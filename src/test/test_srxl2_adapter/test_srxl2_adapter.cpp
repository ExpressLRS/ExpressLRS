#include <unity.h>
#include <string>
#include <vector>
#include "common.h"
#include "CRSFRouter.h"
#include "../test_msp/mock_serial.h"
#include "../../src/rx-serial/devSerialIO.h"
using std::min;

connectionState_e connectionState = connected;
bool connectionHasModelMatch = true, teamraceHasModelMatch = true;
bool crsfBatterySensorDetected = false;
CRSFRouter crsfRouter;
static bool uartReceiving = false;
static uint32_t nowUs;
static unsigned long testMicros() { return nowUs; }

// Substitute only the clock and GPIO boundary; execute the actual adapter/router.
#define SRXL2_ADAPTER_TEST
#include "../../src/rx-serial/RCFrameState.cpp"
#define SRXL2_HARDWARE_RX_BUSY() uartReceiving
#define micros testMicros
#include "../../src/rx-serial/SerialSRXL2.cpp"
#undef micros
#undef SRXL2_HARDWARE_RX_BUSY
#include "../../src/rx-serial/SerialIO.cpp"

class Capture : public CRSFConnector
{
public:
    std::vector<std::vector<uint8_t>> frames;
    void forwardMessage(const crsf_header_t *message) override
    {
        auto p = reinterpret_cast<const uint8_t *>(message);
        frames.emplace_back(p, p + message->frame_size + 2);
    }
} capture;

void setUp()
{
    nowUs = 0;
    connectionState = connected;
    connectionHasModelMatch = teamraceHasModelMatch = true;
    capture.frames.clear();
    uartReceiving = false;
    for (auto &frame : serialRCFrames) frame = SerialRCFrameState{};
    capture.addDevice(CRSF_ADDRESS_RADIO_TRANSMITTER);
    crsfRouter.addConnector(&capture);
}
void tearDown() { crsfRouter.removeConnector(&capture); }

static const uint8_t hello[] = {0xA6,0x21,14,0x40,0x21,10,0,0,0x11,0x22,0x33,0x44,0xC9,0xE7};
static const uint8_t esc[] = {0xA6,0x80,22,0x21,0x20,0,0x30,0x39,4,0xD2,1,0x5E,3,0xE8,0,0xFA,15,0x78,0x64,0x64,0x74,0x74};
static const uint8_t battery[] = {0xA6,0x80,22,0x21,0x42,0,0,0xF6,0xB8,0x0B,0,0,0x39,0x30,1,0x0E,0xD0,0x0E,0,0,0x97,0x9C};

static void send(SerialSRXL2 &driver, uint32_t time)
{
    nowUs = time;
    driver.sendQueuedData(128);
}
static void input(SerialSRXL2 &driver, std::string &in, const uint8_t *data, size_t size, uint32_t time)
{
    nowUs = time;
    in.append(reinterpret_cast<const char *>(data), size);
    driver.processSerialInput();
}
static void establish(SerialSRXL2 &driver, std::string &in, std::string &out)
{
    send(driver, 50000);
    TEST_ASSERT_EQUAL(14, out.size());
    send(driver, 51200);
    input(driver, in, hello, sizeof(hello), 51400);
    send(driver, 51600);
    send(driver, 52800);
    uint32_t channels[16] = {};
    channels[0] = 1811; channels[2] = 992;
    nowUs = 53000;
    driver.sendRCFrame(true, false, channels);
    send(driver, 53000);
    TEST_ASSERT_EQUAL(0xCD, uint8_t(out[out.size() - 15]));
    input(driver, in, esc, sizeof(esc), 55000);
}
static void assert_neutral(const std::string &out)
{
    TEST_ASSERT_EQUAL(1, uint8_t(out[out.size() - 13]));
    TEST_ASSERT_EQUAL(0, uint8_t(out[out.size() - 4]));
    TEST_ASSERT_EQUAL(0x80, uint8_t(out[out.size() - 3]));
}

void test_adapter_ch3_and_all_inhibition_paths()
{
    for (unsigned reason = 0; reason < 4; ++reason)
    {
        std::string in, out;
        StringStream rx(in), tx(out);
        nowUs = 0;
        SerialSRXL2 driver(&tx, &rx, 1);
        establish(driver, in, out);
        uint32_t channels[16] = {};
        channels[0] = 172; channels[2] = 1811;
        nowUs = 63000;
        driver.sendRCFrame(true, false, channels);
        send(driver, 63000);
        TEST_ASSERT_EQUAL(0x54, uint8_t(out[out.size() - 4]));
        TEST_ASSERT_EQUAL(0xD5, uint8_t(out[out.size() - 3]));
        send(driver, 64500);
        if (reason == 0) connectionHasModelMatch = false;
        if (reason == 1) teamraceHasModelMatch = false;
        if (reason == 2) driver.setFailsafe(true);
        if (reason == 3) connectionState = disconnected;
        send(driver, 73000);
        assert_neutral(out);
        connectionState = connected;
        connectionHasModelMatch = teamraceHasModelMatch = true;
    }
}

void test_adapter_missing_frames_do_not_refresh_cached_throttle()
{
    std::string in, out;
    StringStream rx(in), tx(out);
    SerialSRXL2 driver(&tx, &rx, 1);
    establish(driver, in, out);
    uint32_t channels[16] = {};
    channels[2] = 1811;
    nowUs = 60000;
    driver.sendRCFrame(false, true, channels);
    send(driver, 153000);
    assert_neutral(out);
}

void test_adapter_real_crsf_payload_and_budget()
{
    std::string in, out;
    StringStream rx(in), tx(out);
    SerialSRXL2 driver(&tx, &rx, 1);
    establish(driver, in, out);
    send(driver, 100000);
    TEST_ASSERT_EQUAL(1, capture.frames.size());
    const uint8_t wanted[] = {0,123,0,100,0,0,0,0};
    TEST_ASSERT_EQUAL(0x08, capture.frames[0][2]);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(wanted, capture.frames[0].data() + 3, 8);
    TEST_ASSERT_TRUE(crsfBatterySensorDetected);
    send(driver, 199999);
    TEST_ASSERT_EQUAL(1, capture.frames.size());
    input(driver, in, battery, sizeof(battery), 200000);
    for (uint32_t time = 200001; time < 1000000; time += 100000) send(driver, time);
    bool found = false;
    for (const auto &frame : capture.frames)
    {
        if (frame[2] == 8 && frame[6] == 30)
        {
            TEST_ASSERT_EQUAL(0, frame[7]);
            TEST_ASSERT_EQUAL(4, frame[8]);
            TEST_ASSERT_EQUAL(0xD2, frame[9]);
            TEST_ASSERT_EQUAL(0, frame[10]);
            found = true;
        }
    }
    TEST_ASSERT_TRUE(found);
    send(driver, 2300000);
    TEST_ASSERT_FALSE(crsfBatterySensorDetected);
}

void test_adapter_voltage_fallback_and_sentinel_suppression()
{
    std::string in, out;
    StringStream rx(in), tx(out);
    SerialSRXL2 driver(&tx, &rx, 1);
    establish(driver, in, out);
    uint8_t voltageOnly[22];
    std::memcpy(voltageOnly, esc, sizeof(esc));
    for (unsigned i = 6; i < 20; ++i) voltageOnly[i] = 0xFF;
    voltageOnly[8] = 4; voltageOnly[9] = 0xD2;
    uint16_t crc = SRXL2::crc16(voltageOnly, 20);
    voltageOnly[20] = crc >> 8; voltageOnly[21] = crc;
    input(driver, in, voltageOnly, sizeof(voltageOnly), 70000);
    send(driver, 100000);
    const uint8_t wanted[] = {128,0x30,0x34};
    TEST_ASSERT_EQUAL(1, capture.frames.size());
    TEST_ASSERT_EQUAL(0x0E, capture.frames[0][2]);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(wanted, capture.frames[0].data() + 3, 3);
    TEST_ASSERT_FALSE(crsfBatterySensorDetected);
}

void test_rf_resync_between_callbacks_invalidates_actual_frame_state()
{
    std::string in, out;
    StringStream rx(in), tx(out);
    SerialSRXL2 driver(&tx, &rx, 1);
    establish(driver, in, out);
    uint32_t channels[16] = {};
    channels[2] = 1811;
    nowUs = 63000;
    driver.sendRCFrame(true, false, channels);
    send(driver, 63000);
    send(driver, 64500);
    crsfRCFrameAvailable(); // pending old RF frame before TentativeConnection
    connectionState = tentative;
    crsfRCFrameReset();
    connectionHasModelMatch = false;
    // GotConnection runs before any adapter callback can observe tentative.
    connectionState = connected;
    connectionHasModelMatch = true;
    TEST_ASSERT_FALSE(serialRCFrames[0].takeAvailable());
    TEST_ASSERT_EQUAL(CRSF_CHANNEL_VALUE_UNSET, serialChannelValue(CRSF_CHANNEL_VALUE_UNSET, true));
    send(driver, 73000);
    assert_neutral(out); // cached motion must already be revoked
    send(driver, 74500);
    channels[2] = 992;
    crsfRCFrameAvailable();
    nowUs = 80000;
    driver.sendRCFrame(serialRCFrames[0].takeAvailable(), false, channels);
    channels[2] = 1811;
    crsfRCFrameAvailable();
    nowUs = 81000;
    driver.sendRCFrame(serialRCFrames[0].takeAvailable(), false, channels);
    send(driver, 83000);
    TEST_ASSERT_EQUAL(0x54, uint8_t(out[out.size() - 4]));
    TEST_ASSERT_EQUAL(0xD5, uint8_t(out[out.size() - 3]));
}

void test_late_receive_hardware_and_partial_reply_block_transmit()
{
    std::string in, out;
    StringStream rx(in), tx(out);
    SerialSRXL2 driver(&tx, &rx, 1);
    establish(driver, in, out);
    send(driver, 153000); // telemetry grant
    send(driver, 154400);
    const size_t written = out.size();
    uartReceiving = true; // receive start bit/FSM, no completed byte yet
    send(driver, 163000);
    TEST_ASSERT_EQUAL(written, out.size());
    uartReceiving = false;
    send(driver, 163173);
    TEST_ASSERT_EQUAL(written, out.size());
    in.append(reinterpret_cast<const char *>(esc), 3); // delayed FIFO delivery
    send(driver, 163174);
    TEST_ASSERT_EQUAL(written, out.size());
    nowUs = 163200;
    driver.processSerialInput();
    send(driver, 164000);
    TEST_ASSERT_EQUAL(written, out.size());
    input(driver, in, esc + 3, sizeof(esc) - 3, 164100);
    send(driver, 164273);
    TEST_ASSERT_EQUAL(written, out.size());
    send(driver, 164274);
    TEST_ASSERT_EQUAL(written + 16, out.size());
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_adapter_ch3_and_all_inhibition_paths);
    RUN_TEST(test_adapter_missing_frames_do_not_refresh_cached_throttle);
    RUN_TEST(test_adapter_real_crsf_payload_and_budget);
    RUN_TEST(test_adapter_voltage_fallback_and_sentinel_suppression);
    RUN_TEST(test_rf_resync_between_callbacks_invalidates_actual_frame_state);
    RUN_TEST(test_late_receive_hardware_and_partial_reply_block_transmit);
    return UNITY_END();
}

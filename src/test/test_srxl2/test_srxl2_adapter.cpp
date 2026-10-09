#include <unity.h>
#include <cstring>
#include <string>
#include <vector>
#include "SX12xxDriverCommon.h" // Load the native hardware boundary first.
#define PLATFORM_ESP32 // Include the real secondary enum without ESP-IDF hardware.
#include "common.h"
#undef PLATFORM_ESP32
#include "CRSFRouter.h"
#include "RXOTAConnector.h"
#include "binary_serial.h"
using std::min;

connectionState_e connectionState = connected;
bool connectionHasModelMatch = true, teamraceHasModelMatch = true;
bool crsfBatterySensorDetected = false;
uint32_t ChannelData[CRSF_NUM_CHANNELS] = {};
CRSFRouter crsfRouter;
static bool uartReceiving = false;
static bool secondaryReceiving = false;
static struct {
    eSerialProtocol primary = PROTOCOL_SRXL2;
    eSerial1Protocol secondary = PROTOCOL_SERIAL1_OFF;
    eSerialProtocol GetSerialProtocol() const { return primary; }
    eSerial1Protocol GetSerial1Protocol() const { return secondary; }
} config;
static struct { bool is_airport = false; } firmwareOptions;
static uint32_t nowUs;
static unsigned long testMicros() { return nowUs; }
static bool busDriving, txDrained, releasedBeforeDrain;
static bool autoTxDone = true;
static bool replyBlocked;
static UartStartupSpy uartStartup;
static UartStartupSpy uartStartup1;
static constexpr uint32_t SERIAL_8N1 = 0x800001c;
static void (*txDoneInterrupt)(void *);
static void *txDoneArgument;
static bool irqAllocationSucceeds = true;
static bool installTxInterrupt(void (*handler)(void *), void *argument)
{
    if (!irqAllocationSucceeds) return false;
    txDoneInterrupt = handler;
    txDoneArgument = argument;
    return true;
}
static std::string *immediateReplyInput;
static std::string immediateReply;
static std::string *startupInput;
static std::string startupAnnouncement, startupReply;
static uint32_t startupInjectUs;
static uint32_t startupSecondInjectUs;
static uint32_t startupRxBusyUntilUs;
static std::string startupSecondAnnouncement;
static void startupYield()
{
    nowUs += 1000;
    if (startupRxBusyUntilUs && nowUs == startupRxBusyUntilUs) uartReceiving = false;
    if (startupInput && nowUs == startupInjectUs) startupInput->append(startupAnnouncement);
    if (startupInput && nowUs == startupSecondInjectUs) startupInput->append(startupSecondAnnouncement);
}
static void startBusTransmit() { busDriving = true; txDrained = false; }
static void drainBusTransmit()
{
    if (!busDriving) return;
    txDrained = true;
    if (txDoneInterrupt) txDoneInterrupt(txDoneArgument);
    if (immediateReplyInput)
    {
        replyBlocked |= busDriving;
        if (!busDriving) immediateReplyInput->append(immediateReply);
    }
}
static void releaseBusTransmit()
{
    releasedBeforeDrain |= !txDrained;
    busDriving = false;
}

// Substitute only the clock and GPIO boundary; execute the actual adapter/router.
#define SRXL2_ADAPTER_TEST
#define SRXL2_EARLY_STARTUP_PIN 3
#define SRXL2_EARLY_BEGIN_TX() startBusTransmit()
#define SRXL2_EARLY_RELEASE_TX() releaseBusTransmit()
#define SRXL2_EARLY_YIELD() startupYield()
#define SRXL2_HARDWARE_RX_BUSY() (port == 0 ? uartReceiving : secondaryReceiving)
#define SRXL2_INSTALL_TX_IRQ(handler, argument) installTxInterrupt(handler, argument)
#define SRXL2_REMOVE_TX_IRQ() (txDoneInterrupt = nullptr)
#define SRXL2_BEGIN_TX() startBusTransmit()
#define SRXL2_POLL_TX_IRQ() do { if (autoTxDone) drainBusTransmit(); } while (0)
#define SRXL2_RELEASE_TX() releaseBusTransmit()
#define micros testMicros
#define Serial uartStartup
#define Serial1 uartStartup1
#include "../../src/rx-serial/SerialSRXL2.cpp"
#undef Serial
#undef Serial1
#undef micros
#undef SRXL2_HARDWARE_RX_BUSY
#undef SRXL2_BEGIN_TX
#undef SRXL2_POLL_TX_IRQ
#undef SRXL2_RELEASE_TX
#include "../../src/rx-serial/SerialIO.cpp"

class Capture : public RXOTAConnector
{
public:
    std::vector<std::vector<uint8_t>> frames;
    void forwardMessage(const crsf_header_t *message) override
    {
        RXOTAConnector::forwardMessage(message);
        auto p = reinterpret_cast<const uint8_t *>(message);
        frames.emplace_back(p, p + message->frame_size + 2);
    }
} capture;

void setUp()
{
    startupState = SRXL2StartupState();
    nowUs = 0;
    connectionState = connected;
    connectionHasModelMatch = teamraceHasModelMatch = true;
    capture.frames.clear();
    uint8_t queuedSize, queued[CRSF_MAX_PACKET_LEN];
    while (capture.GetNextPayload(&queuedSize, queued)) {}
    uartReceiving = false;
    secondaryReceiving = false;
    config.primary = PROTOCOL_SRXL2;
    config.secondary = PROTOCOL_SERIAL1_OFF;
    firmwareOptions.is_airport = false;
    irqAllocationSucceeds = true;
    busDriving = txDrained = releasedBeforeDrain = false;
    autoTxDone = true;
    replyBlocked = false;
    uartStartup = UartStartupSpy();
    uartStartup1 = UartStartupSpy();
    txDoneInterrupt = nullptr;
    immediateReplyInput = nullptr;
    immediateReply.clear();
    startupInput = nullptr;
    startupAnnouncement.clear();
    startupReply.clear();
    startupInjectUs = 0;
    startupSecondInjectUs = 0;
    startupRxBusyUntilUs = 0;
    startupSecondAnnouncement.clear();
    for (auto &channel : ChannelData) channel = CRSF_CHANNEL_VALUE_UNSET;
    capture.addDevice(CRSF_ADDRESS_RADIO_TRANSMITTER);
    crsfRouter.addConnector(&capture);
}
void tearDown() { crsfRouter.removeConnector(&capture); }

static const uint8_t hello[] = {0xA6,0x21,14,0x40,0x21,10,0,0,0x11,0x22,0x33,0x44,0xC9,0xE7};
static const uint8_t esc[] = {0xA6,0x80,22,0x21,0x20,0,0x30,0x39,4,0xD2,1,0x5E,3,0xE8,0,0xFA,15,0x78,0x64,0x64,0x74,0x74};
static const uint8_t battery[] = {0xA6,0x80,22,0x21,0x42,0,0,0xF6,0xB8,0x0B,0,0,0x39,0x30,1,0x0E,0xD0,0x0E,0,0,0x97,0x9C};
static const uint8_t cells[] = {0xA6,0x80,22,0x21,0x42,0,0x10,0x19,1,0x10,2,0x10,3,0x10,0xFF,0xFF,0,0,0xFF,0xFF,0x28,0xD3};
static const uint8_t identity[] = {0xA6,0x80,22,0x21,0x42,0,0x80,1,3,1,1,0,0x11,0x22,0x33,0x44,0x55,0x66,0x77,0x88,0x4D,0x81};

class StartupTxStream : public BinaryStringStream
{
public:
    using BinaryStringStream::BinaryStringStream;
    uint32_t startedUs = 0;
    size_t write(const uint8_t *bytes, size_t size) override
    {
        startedUs = nowUs;
        return BinaryStringStream::write(bytes, size);
    }
    void flush() override
    {
        nowUs += 1216;
        txDrained = true;
        if (startupInput) startupInput->append(startupReply);
    }
};

void test_early_startup_ack_handles_a_fresh_announcement_and_releases_the_wire()
{
    const uint8_t announcement[] = {0xA6,0x21,14,0x40,0,10,0,0,0,0,0,1,0x38,0x4E};
    for (uint32_t start : {50000u, 0xFFFFF000u, uint32_t(0u - 11000u)})
    {
        nowUs = start;
        busDriving = txDrained = releasedBeforeDrain = false;
        std::string in, out;
        BinaryStringStream rx(in);
        StartupTxStream tx(out);
        startupInput = &in;
        startupInjectUs = start + 10000;
        startupAnnouncement.assign(reinterpret_cast<const char *>(announcement), sizeof(announcement));
        startupReply.assign(reinterpret_cast<const char *>(hello), sizeof(hello));
        SRXL2StartupState state;
        listenForEarlyESC(rx, tx, 0x12345678, state);
        TEST_ASSERT_EQUAL_UINT32(start + 11000, tx.startedUs); // Two-character idle before ACK.
        TEST_ASSERT_EQUAL_UINT32(start + 12216, state.ackEndUs);
        TEST_ASSERT_EQUAL_UINT32(start + 32216, nowUs);
        TEST_ASSERT_EQUAL(14, out.size()); // ACK only; no final broadcast or control packets.
        TEST_ASSERT_EQUAL_HEX8(0x40, uint8_t(out[4]));
        TEST_ASSERT_EQUAL_HEX8(0, uint8_t(out[6]));
        TEST_ASSERT_EQUAL_HEX8(3, uint8_t(out[7]));
        TEST_ASSERT_EQUAL(0, rx.available()); // Both the announcement and addressed reply were consumed.
        TEST_ASSERT_FALSE(busDriving);
        TEST_ASSERT_FALSE(releasedBeforeDrain);
    }
}

void test_early_startup_does_not_ack_corrupt_or_nonannouncement_frames()
{
    const uint8_t announcement[] = {0xA6,0x21,14,0x40,0,10,0,0,0,0,0,1,0x38,0x4E};
    nowUs = 50000;
    std::string in(reinterpret_cast<const char *>(announcement), sizeof(announcement)), out;
    in.back() ^= 1; // Buffered corruption must not trigger an ACK either.
    BinaryStringStream rx(in);
    StartupTxStream tx(out);
    startupInput = &in;
    startupInjectUs = 60000;
    startupAnnouncement.assign(reinterpret_cast<const char *>(announcement), sizeof(announcement));
    startupAnnouncement.back() ^= 1; // Corrupt CRC, then a valid addressed reply, then a partial announcement.
    startupAnnouncement.append(reinterpret_cast<const char *>(hello), sizeof(hello));
    startupAnnouncement.append(reinterpret_cast<const char *>(announcement), 7);
    SRXL2StartupState state;
    listenForEarlyESC(rx, tx, 0x12345678, state);
    TEST_ASSERT_TRUE(out.empty());
    TEST_ASSERT_EQUAL(0, state.ackEndUs);
    TEST_ASSERT_EQUAL_UINT32(300000, nowUs);
    TEST_ASSERT_FALSE(busDriving);
}

void test_early_startup_acknowledges_buffered_announcements_after_hardware_idle()
{
    const uint8_t announcement[] = {0xA6,0x21,14,0x40,0,10,0,0,0,0,0,1,0x38,0x4E};
    nowUs = 50000;
    std::string in, out;
    for (unsigned i = 0; i < 9; ++i) in.append(reinterpret_cast<const char *>(announcement), sizeof(announcement));
    BinaryStringStream rx(in);
    StartupTxStream tx(out);
    startupInput = &in;
    startupReply.assign(reinterpret_cast<const char *>(hello), sizeof(hello));
    uartReceiving = true;
    startupRxBusyUntilUs = 53000;
    SRXL2StartupState state;
    listenForEarlyESC(rx, tx, 0x12345678, state);
    TEST_ASSERT_EQUAL(14, out.size());
    TEST_ASSERT_EQUAL_UINT32(54000, tx.startedUs);
    TEST_ASSERT_EQUAL_UINT32(55216, state.ackEndUs);
    TEST_ASSERT_EQUAL(0, rx.available()); // Queued announcements and the fresh addressed reply were consumed.
    TEST_ASSERT_FALSE(busDriving);
    TEST_ASSERT_FALSE(releasedBeforeDrain);
}

void test_early_startup_recovers_after_unsuitable_input_before_an_announcement()
{
    const uint8_t announcement[] = {0xA6,0x21,14,0x40,0,10,0,0,0,0,0,1,0x38,0x4E};
    for (const std::string prefix : {std::string("\xA6\x21\x50", 3), std::string(reinterpret_cast<const char *>(hello), sizeof(hello))})
    {
        nowUs = 50000;
        std::string in, out;
        BinaryStringStream rx(in);
        StartupTxStream tx(out);
        startupInput = &in;
        startupInjectUs = 60000;
        startupAnnouncement = prefix;
        startupSecondInjectUs = 100000;
        startupSecondAnnouncement.assign(reinterpret_cast<const char *>(announcement), sizeof(announcement));
        startupReply.clear();
        SRXL2StartupState state;
        listenForEarlyESC(rx, tx, 0x12345678, state);
        TEST_ASSERT_EQUAL_UINT32(101000, tx.startedUs);
        TEST_ASSERT_EQUAL(14, out.size());
        TEST_ASSERT_EQUAL_HEX8(0x40, uint8_t(out[4]));
        TEST_ASSERT_FALSE(busDriving);
    }
}

static void send(SerialSRXL2 &driver, uint32_t time);
static void assert_neutral(const std::string &out, uint8_t command);
void test_normal_driver_finishes_acknowledged_startup_without_an_addressed_reply()
{
    const uint8_t announcement[] = {0xA6,0x21,14,0x40,0,10,0,0,0,0,0,1,0x38,0x4E};
    nowUs = 50000;
    std::string bootIn(reinterpret_cast<const char *>(announcement), sizeof(announcement)), bootOut;
    BinaryStringStream bootRx(bootIn);
    StartupTxStream bootTx(bootOut);
    startupInput = &bootIn;
    startupReply.clear(); // Spec/reference permits no duplicate addressed reply after an announcement.
    listenForEarlyESC(bootRx, bootTx, 0x12345678, startupState);
    TEST_ASSERT_EQUAL(14, bootOut.size());

    connectionState = wifiUpdate;
    nowUs = 200000;
    std::string in, out;
    BinaryStringStream rx(in), tx(out);
    {
        SerialSRXL2 wrongPin(&tx, &rx, 1);
        send(wrongPin, 201000);
        TEST_ASSERT_TRUE(out.empty()); // GPIO3's early ACK does not describe GPIO1's bus.
    }
    nowUs = 210000;
    {
        SerialSRXL2 driver(&tx, &rx, 3);
        send(driver, 211000);
        TEST_ASSERT_EQUAL(14, out.size());
        TEST_ASSERT_EQUAL_HEX8(0xFF, uint8_t(out[4]));
        send(driver, 212216);
        send(driver, 232216);
        assert_neutral(out, 1); // Discovery knowledge must not grant RF/throttle permission.
    }
    const size_t sent = out.size();
    nowUs = 300000;
    SerialSRXL2 replacement(&tx, &rx, 3);
    send(replacement, 301000);
    TEST_ASSERT_EQUAL(sent, out.size()); // The startup handoff is consumed only once.
}

void test_smart_startup_attaches_receive_without_driving_the_signal_pin()
{
    for (int8_t signal : {1, 3})
    {
        std::string in, out;
        BinaryStringStream rx(in), tx(out);
        SerialSRXL2 driver(&tx, &rx, signal);
        TEST_ASSERT_EQUAL_INT8(signal, uartStartup.rxPin);
        TEST_ASSERT_EQUAL_INT8(-1, uartStartup.txPin);
        TEST_ASSERT_EQUAL_UINT32(115200, uartStartup.baud);
        TEST_ASSERT_EQUAL_HEX32(SERIAL_8N1, uartStartup.format);
        TEST_ASSERT_FALSE(uartStartup.inverted);
        TEST_ASSERT_EQUAL(0, uartStartup.txBufferSize);
        TEST_ASSERT_EQUAL(1, uartStartup.rxThreshold);
        TEST_ASSERT_TRUE(out.empty());
    }
}

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

static void deliver(SerialSRXL2 &driver, bool available, uint32_t *channels)
{
    std::memcpy(ChannelData, channels, sizeof(ChannelData));
    driver.sendRCFrame(available, false, channels);
}
static void establish(SerialSRXL2 &driver, std::string &in, std::string &out)
{
    // Model an ESC already advertising while the receiver starts listening.
    input(driver, in, hello, sizeof(hello), 1400);
    send(driver, 1600);
    TEST_ASSERT_EQUAL(14, out.size());
    send(driver, 2800);
    send(driver, 23000);
    send(driver, 24400);
    send(driver, 43000);
    send(driver, 44400);
    uint32_t channels[16] = {};
    channels[0] = 1811; channels[2] = 992;
    nowUs = 53000;
    deliver(driver, true, channels);
    send(driver, 53000);
    TEST_ASSERT_EQUAL(0xCD, uint8_t(out[out.size() - 15]));
    nowUs = 54000;
    deliver(driver, true, channels); // first cached callback was discarded
    input(driver, in, esc, sizeof(esc), 55000);
}
static void assert_neutral(const std::string &out, uint8_t command = 1)
{
    TEST_ASSERT_EQUAL(command, uint8_t(out[out.size() - 13]));
    TEST_ASSERT_EQUAL(0, uint8_t(out[out.size() - 4]));
    TEST_ASSERT_EQUAL(0x80, uint8_t(out[out.size() - 3]));
}

void test_adapter_ch3_and_all_inhibition_paths()
{
    for (unsigned reason = 0; reason < 5; ++reason)
    {
        std::string in, out;
        BinaryStringStream rx(in), tx(out);
        nowUs = 0;
        SerialSRXL2 driver(&tx, &rx, 1);
        establish(driver, in, out);
        uint32_t channels[16] = {};
        channels[0] = 172; channels[2] = 1811;
        nowUs = 63000;
        deliver(driver, true, channels);
        send(driver, 63000);
        TEST_ASSERT_EQUAL(0x54, uint8_t(out[out.size() - 4]));
        TEST_ASSERT_EQUAL(0xD5, uint8_t(out[out.size() - 3]));
        send(driver, 64500);
        if (reason == 0) connectionHasModelMatch = false;
        if (reason == 1) teamraceHasModelMatch = false;
        if (reason == 2) driver.setFailsafe(true);
        if (reason == 3) connectionState = disconnected;
        if (reason == 4) connectionState = wifiUpdate;
        send(driver, 73000);
        send(driver, 84500); // Complete the reserved telemetry reply window before safety TX.
        assert_neutral(out);
        connectionState = connected;
        connectionHasModelMatch = teamraceHasModelMatch = true;
    }
}

void test_adapter_missing_frames_do_not_refresh_cached_throttle()
{
    std::string in, out;
    BinaryStringStream rx(in), tx(out);
    SerialSRXL2 driver(&tx, &rx, 1);
    establish(driver, in, out);
    uint32_t channels[16] = {};
    channels[2] = 1811;
    nowUs = 60000;
    deliver(driver, false, channels);
    send(driver, 154000); // 100 ms after the accepted neutral sample
    assert_neutral(out);
}

void test_adapter_missed_flag_sends_fade_until_sample_expires()
{
    std::string in, out;
    BinaryStringStream rx(in), tx(out);
    SerialSRXL2 driver(&tx, &rx, 1);
    establish(driver, in, out);
    uint32_t channels[16] = {};
    channels[2] = 1811;
    nowUs = 63000;
    deliver(driver, true, channels);
    send(driver, 63000);
    send(driver, 64500);
    for (uint32_t time : {93000u, 123000u})
    {
        nowUs = time;
        driver.sendRCFrame(false, true, channels);
        const size_t before = out.size();
        send(driver, time);
        TEST_ASSERT_EQUAL(before + 14, out.size());
        TEST_ASSERT_EQUAL(0, uint8_t(out[before + 3]));
        const uint8_t noChannels[] = {0,0,0,0};
        TEST_ASSERT_EQUAL_UINT8_ARRAY(noChannels, reinterpret_cast<const uint8_t *>(out.data()) + before + 8, 4);
        send(driver, time + 1300);
    }
    nowUs = 163000;
    driver.sendRCFrame(false, true, channels);
    const size_t before = out.size();
    send(driver, 163000);
    TEST_ASSERT_EQUAL(before + 16, out.size());
    assert_neutral(out); // Missed callbacks cannot refresh the 100 ms sample deadline.
    input(driver, in, esc, sizeof(esc), 165000);
    nowUs = 173000;
    deliver(driver, true, channels);
    send(driver, 173000);
    send(driver, 183000);
    assert_neutral(out, 0); // Recovery still requires a fresh centered sample.
}

void test_missing_raw_ch3_is_not_an_extreme_throttle_snapshot()
{
    std::string in, out;
    BinaryStringStream rx(in), tx(out);
    SerialSRXL2 driver(&tx, &rx, 1);
    establish(driver, in, out);
    uint32_t channels[16] = {};
    channels[2] = 1811;
    ChannelData[2] = 1811;
    nowUs = 63000;
    deliver(driver, true, channels);
    send(driver, 63000);
    send(driver, 64500);
    ChannelData[2] = CRSF_CHANNEL_VALUE_UNSET;
    channels[2] = CRSF_CHANNEL_VALUE_EXT_MIN; // unchanged upstream snapshot conversion
    nowUs = 70000;
    driver.sendRCFrame(true, false, channels);
    send(driver, 73000);
    send(driver, 84500);
    assert_neutral(out);
}

void test_adapter_real_crsf_payload_and_budget()
{
    std::string in, out;
    BinaryStringStream rx(in), tx(out);
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

void test_esc_sensors_reach_the_real_elrs_downlink_queue()
{
    std::string in, out;
    BinaryStringStream rx(in), tx(out);
    SerialSRXL2 driver(&tx, &rx, 3);
    establish(driver, in, out);
    input(driver, in, cells, sizeof(cells), 60000);
    input(driver, in, battery, sizeof(battery), 70000);
    input(driver, in, identity, sizeof(identity), 80000);
    // Complete CRSF frames, including independently calculated DVB-S2 CRCs:
    // 12.3 V/3.0 A/1234 mAh; 123450 electrical RPM; 35/25/-10 C; 6 V BEC; cells.
    const uint8_t expected[][12] = {
        {0xC8,0x0A,0x08,0x00,0x7B,0x00,0x1E,0x00,0x04,0xD2,0x00,0x63},
        {0xC8,0x06,0x0C,0x00,0x01,0xE2,0x3A,0xA8},
        {0xC8,0x05,0x0D,0x00,0x01,0x5E,0xBF},
        {0xC8,0x05,0x0D,0x01,0x00,0xFA,0x82},
        {0xC8,0x05,0x0D,0x02,0xFF,0x9C,0x12},
        {0xC8,0x05,0x0E,0x81,0x17,0x70,0xBD},
        {0xC8,0x09,0x0E,0x00,0x10,0x01,0x10,0x02,0x10,0x03,0x95},
    };
    const uint8_t lengths[] = {12,8,7,7,7,7,11};
    uint8_t size = 0, queued[CRSF_MAX_PACKET_LEN];
    for (unsigned sensor = 0; sensor < 7; ++sensor)
    {
        send(driver, 100000 + sensor * 100000);
        TEST_ASSERT_TRUE(capture.GetNextPayload(&size, queued));
        TEST_ASSERT_EQUAL(lengths[sensor], size);
        TEST_ASSERT_EQUAL_UINT8_ARRAY(expected[sensor], queued, size);
        TEST_ASSERT_FALSE(capture.GetNextPayload(&size, queued));
    }
}

void test_adapter_voltage_fallback_and_sentinel_suppression()
{
    std::string in, out;
    BinaryStringStream rx(in), tx(out);
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

void test_rf_resync_drops_stale_callback_without_changing_core_latches()
{
    std::string in, out;
    BinaryStringStream rx(in), tx(out);
    SerialSRXL2 driver(&tx, &rx, 1);
    establish(driver, in, out);
    uint32_t channels[16] = {};
    channels[2] = 1811;
    nowUs = 63000;
    deliver(driver, true, channels);
    send(driver, 63000);
    send(driver, 64500);
    connectionState = tentative;
    SerialSRXL2::onRFReset();
    for (auto &channel : ChannelData) channel = CRSF_CHANNEL_VALUE_UNSET;
    connectionHasModelMatch = false;
    // GotConnection runs before any adapter callback can observe tentative.
    connectionState = connected;
    connectionHasModelMatch = true;
    // Upstream's pending flag remains true; its snapshot substitutes minimum.
    channels[2] = CRSF_CHANNEL_VALUE_EXT_MIN;
    nowUs = 70000;
    driver.sendRCFrame(true, false, channels);
    send(driver, 73000);
    send(driver, 84500);
    assert_neutral(out); // cached motion must already be revoked
    send(driver, 86000);
    channels[2] = 992;
    nowUs = 90000;
    deliver(driver, true, channels);
    channels[2] = 1811;
    nowUs = 91000;
    deliver(driver, true, channels);
    send(driver, 104500);
    TEST_ASSERT_EQUAL(0x54, uint8_t(out[out.size() - 4]));
    TEST_ASSERT_EQUAL(0xD5, uint8_t(out[out.size() - 3]));
}

void test_late_receive_hardware_and_partial_reply_block_transmit()
{
    std::string in, out;
    BinaryStringStream rx(in), tx(out);
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

void test_first_pending_neutral_after_rf_reset_cannot_release_motion()
{
    std::string in, out;
    BinaryStringStream rx(in), tx(out);
    SerialSRXL2 driver(&tx, &rx, 1);
    establish(driver, in, out);
    uint32_t channels[16] = {};
    channels[2] = 1811;
    nowUs = 63000;
    deliver(driver, true, channels);
    send(driver, 63000);
    send(driver, 64500);
    SerialSRXL2::onRFReset();
    channels[2] = 992;
    nowUs = 66000;
    deliver(driver, true, channels); // a stale pending callback must be discarded
    channels[2] = 1811;
    nowUs = 67000;
    deliver(driver, true, channels);
    send(driver, 73000);
    send(driver, 84500);
    assert_neutral(out, 0);
    send(driver, 86000);
    input(driver, in, esc, sizeof(esc), 88000);
    channels[2] = 992;
    nowUs = 90000;
    deliver(driver, true, channels);
    channels[2] = 1811;
    nowUs = 91000;
    deliver(driver, true, channels);
    send(driver, 104500);
    TEST_ASSERT_EQUAL(0x54, uint8_t(out[out.size() - 4]));
    TEST_ASSERT_EQUAL(0xD5, uint8_t(out[out.size() - 3]));
}

void test_live_driver_revokes_motion_when_another_protocol_is_selected()
{
    std::string in, out;
    BinaryStringStream rx(in), tx(out);
    SerialSRXL2 driver(&tx, &rx, 1);
    establish(driver, in, out);
    uint32_t channels[16] = {};
    channels[2] = 1811;
    nowUs = 63000;
    deliver(driver, true, channels);
    send(driver, 63000);
    send(driver, 64500);

    // Core can select MAVLink and defer replacing this object for 100 ms.
    // A complete RF resync can occur before our callback, with no SRXL2 hook.
    config.primary = PROTOCOL_CRSF;
    send(driver, 73000);
    send(driver, 84500);
    assert_neutral(out);
    send(driver, 86000);
    config.primary = PROTOCOL_SRXL2;
    nowUs = 90000;
    deliver(driver, true, channels);
    send(driver, 104500);
    assert_neutral(out, 0); // changing back does not restore an old release latch
    send(driver, 106000);
    input(driver, in, esc, sizeof(esc), 108000);
    channels[2] = 992;
    nowUs = 110000;
    deliver(driver, true, channels);
    channels[2] = 1811;
    nowUs = 111000;
    deliver(driver, true, channels);
    send(driver, 124500);
    TEST_ASSERT_EQUAL(0x54, uint8_t(out[out.size() - 4]));
    TEST_ASSERT_EQUAL(0xD5, uint8_t(out[out.size() - 3]));
}

void test_tx_interrupt_releases_bus_while_main_loop_is_suspended()
{
    class WireStream : public BinaryStringStream
    {
    public:
        using BinaryStringStream::BinaryStringStream;
        bool wroteWithoutDrive = false;
        size_t write(const uint8_t *bytes, size_t size) override
        {
            wroteWithoutDrive |= !busDriving;
            const size_t written = BinaryStringStream::write(bytes, size);
            // Hardware finishes and the ESC replies before write() returns.
            drainBusTransmit();
            return written;
        }
    };
    std::string in, out;
    BinaryStringStream rx(in);
    WireStream tx(out);
    SerialSRXL2 driver(&tx, &rx, 1);
    immediateReplyInput = &in;
    immediateReply.assign(reinterpret_cast<const char *>(hello), sizeof(hello));
    send(driver, 50000);
    immediateReplyInput = nullptr;
    TEST_ASSERT_FALSE(tx.wroteWithoutDrive);
    TEST_ASSERT_TRUE(txDrained);
    TEST_ASSERT_FALSE(releasedBeforeDrain);
    TEST_ASSERT_FALSE(busDriving);
    TEST_ASSERT_FALSE(replyBlocked);
    driver.processSerialInput(); // Reply already buffered when the last stop bit ended.
    send(driver, 50174);
    TEST_ASSERT_EQUAL(28, out.size());
    TEST_ASSERT_EQUAL_UINT8(0xFF, uint8_t(out[18])); // Accepted ESC hello causes a broadcast.
}

void test_split_echo_and_buffered_reply_survive_delayed_tx_done_callback()
{
    for (unsigned split = 0; split <= 16; ++split)
    {
        for (uint32_t delay : {0u, 4000u})
        {
            std::string in, out;
            BinaryStringStream rx(in), tx(out);
            nowUs = 0;
            autoTxDone = true;
            SerialSRXL2 driver(&tx, &rx, 1);
            establish(driver, in, out);
            uint32_t channels[16] = {};
            channels[2] = 198; // This grant's independently calculated CRC is 36 A6.
            nowUs = 153000;
            deliver(driver, true, channels);
            autoTxDone = false; // Hardware FIFO/shifter remains busy during the echo prefix.
            const size_t before = out.size();
            send(driver, 153000);
            TEST_ASSERT_EQUAL(before + 16, out.size());
            const std::string echo = out.substr(before);
            TEST_ASSERT_EQUAL_HEX8(0x40, uint8_t(echo[4]));
            TEST_ASSERT_EQUAL_HEX8(0x36, uint8_t(echo[14]));
            TEST_ASSERT_EQUAL_HEX8(0xA6, uint8_t(echo[15]));
            input(driver, in, reinterpret_cast<const uint8_t *>(echo.data()), split, 154000);
            TEST_ASSERT_TRUE(busDriving);
            nowUs = 154390;
            drainBusTransmit(); // TX_DONE releases GPIO while the main loop is suspended.
            TEST_ASSERT_FALSE(busDriving);
            TEST_ASSERT_FALSE(releasedBeforeDrain);
            in.append(echo.data() + split, echo.size() - split);
            in.append(reinterpret_cast<const char *>(hello), sizeof(hello));
            nowUs = 154390 + delay;
            driver.processSerialInput(); // Contiguous wire bytes may wait >2500 us for this callback.
            send(driver, nowUs + 173);
            TEST_ASSERT_EQUAL(before + 16, out.size());
            send(driver, nowUs + 1);
            TEST_ASSERT_EQUAL(before + 30, out.size());
            TEST_ASSERT_EQUAL_HEX8(0xFF, uint8_t(out[out.size() - 10]));
            drainBusTransmit();
        }
    }
}

void test_adapter_preserves_delayed_genuine_reply_after_complete_echo()
{
    std::string in, out;
    BinaryStringStream rx(in), tx(out);
    SerialSRXL2 driver(&tx, &rx, 1);
    send(driver, 50000);
    nowUs = 51216;
    drainBusTransmit();
    in.append(out); // Complete outgoing echo has reached the UART0 buffer.
    in.append(reinterpret_cast<const char *>(hello), 4);
    nowUs = 51400;
    driver.processSerialInput();
    input(driver, in, hello + 4, sizeof(hello) - 4, 55400); // Late callback, contiguous wire reply.
    send(driver, 55573);
    TEST_ASSERT_EQUAL(14, out.size());
    send(driver, 55574);
    TEST_ASSERT_EQUAL(28, out.size());
    TEST_ASSERT_EQUAL_HEX8(0xFF, uint8_t(out[18]));
}

void test_secondary_startup_fifo_prefix_and_handoff_match_port_and_pin()
{
    const uint8_t announcement[] = {0xA6,0x21,14,0x40,0,10,0,0,0,0,0,1,0x38,0x4E};
    nowUs = 50000;
    uartReceiving = true;
    std::string bootIn, bootOut;
    BinaryStringStream bootRx(bootIn);
    StartupTxStream bootTx(bootOut);
    listenForEarlyESC(bootRx, bootTx, 0x12345678, startupState, 1, 14, announcement, sizeof(announcement));
    TEST_ASSERT_EQUAL(14, bootOut.size());
    TEST_ASSERT_EQUAL_UINT32(51000, bootTx.startedUs);
    TEST_ASSERT_FALSE(busDriving);
    connectionState = wifiUpdate;
    uartReceiving = false;
    std::string in, out;
    BinaryStringStream rx(in), tx(out);
    nowUs = 200000;
    {
        SerialSRXL2 wrongPort(&tx, &rx, 14, 0);
        send(wrongPort, 201000);
        TEST_ASSERT_TRUE(out.empty());
    }
    nowUs = 210000;
    {
        SerialSRXL2 wrongPin(&tx, &rx, 15, 1);
        send(wrongPin, 211000);
        TEST_ASSERT_TRUE(out.empty());
    }
    nowUs = 220000;
    {
        SerialSRXL2 driver(&tx, &rx, 14, 1);
        send(driver, 221000);
        TEST_ASSERT_EQUAL(14, out.size());
        TEST_ASSERT_EQUAL_HEX8(0xFF, uint8_t(out[4]));
        send(driver, 222216);
        send(driver, 242216);
        assert_neutral(out);
    }
    const size_t written = out.size();
    nowUs = 300000;
    SerialSRXL2 replacement(&tx, &rx, 14, 1);
    send(replacement, 301000);
    TEST_ASSERT_EQUAL(written, out.size());
}

void test_secondary_startup_rejects_a_corrupt_fifo_prefix()
{
    uint8_t corrupt[] = {0xA6,0x21,14,0x40,0,10,0,0,0,0,0,1,0x38,0x4F};
    nowUs = 50000;
    std::string in, out;
    BinaryStringStream rx(in);
    StartupTxStream tx(out);
    listenForEarlyESC(rx, tx, 0x12345678, startupState, 1, 14, corrupt, sizeof(corrupt));
    TEST_ASSERT_TRUE(out.empty());
    TEST_ASSERT_EQUAL(0, startupState.ackEndUs);
    TEST_ASSERT_EQUAL_UINT32(300000, nowUs);
}

void test_secondary_uses_its_uart_and_ignores_primary_receive_activity()
{
    std::string in, out;
    BinaryStringStream rx(in), tx(out);
    SerialSRXL2 driver(&tx, &rx, 14, 1);
    TEST_ASSERT_EQUAL(0, uartStartup.begins);
    TEST_ASSERT_EQUAL(1, uartStartup1.begins);
    TEST_ASSERT_EQUAL_INT8(14, uartStartup1.rxPin);
    TEST_ASSERT_EQUAL_INT8(-1, uartStartup1.txPin);
    TEST_ASSERT_EQUAL(115200, uartStartup1.baud);
    uartReceiving = true;
    send(driver, 50000);
    TEST_ASSERT_EQUAL(14, out.size());
    send(driver, 51216);
    secondaryReceiving = true;
    send(driver, 100000);
    TEST_ASSERT_EQUAL(14, out.size());
    secondaryReceiving = false;
    send(driver, 100173);
    TEST_ASSERT_EQUAL(14, out.size());
    send(driver, 100174);
    TEST_ASSERT_EQUAL(28, out.size());
    TEST_ASSERT_FALSE(releasedBeforeDrain);
}

void test_secondary_preserves_neutral_and_all_inhibition_paths()
{
    for (unsigned reason = 0; reason < 7; ++reason)
    {
        nowUs = 0;
        connectionState = connected;
        connectionHasModelMatch = teamraceHasModelMatch = true;
        config.primary = PROTOCOL_CRSF;
        config.secondary = PROTOCOL_SERIAL1_SRXL2;
        uartReceiving = true; // Busy CRSF UART0 must not delay the Smart bus.
        std::string in, out;
        BinaryStringStream rx(in), tx(out);
        SerialSRXL2 driver(&tx, &rx, 14, 1);
        establish(driver, in, out);
        uint32_t channels[16] = {};
        channels[2] = 1811;
        nowUs = 63000;
        deliver(driver, true, channels);
        send(driver, 63000);
        TEST_ASSERT_EQUAL_HEX8(0xD5, uint8_t(out[out.size() - 3]));
        send(driver, 64500);
        if (reason == 0) connectionHasModelMatch = false;
        if (reason == 1) teamraceHasModelMatch = false;
        if (reason == 2) driver.setFailsafe(true);
        if (reason == 3) connectionState = wifiUpdate;
        if (reason == 4) config.secondary = PROTOCOL_SERIAL1_CRSF;
        if (reason == 5) ChannelData[2] = CRSF_CHANNEL_VALUE_UNSET;
        if (reason == 6) SerialSRXL2::onRFReset();
        send(driver, 73000);
        send(driver, 84500);
        assert_neutral(out);
        TEST_ASSERT_EQUAL(0, uartStartup.begins);
    }
}

void test_rejected_owner_leaves_uart_irq_input_and_telemetry_with_first_owner()
{
    std::string in, out;
    BinaryStringStream rx(in), tx(out);
    {
        SerialSRXL2 first(&tx, &rx, 1);
        establish(first, in, out);
        send(first, 100000);
        TEST_ASSERT_TRUE(crsfBatterySensorDetected);
        auto handler = txDoneInterrupt;
        auto argument = txDoneArgument;
        const size_t written = out.size();
        in.append(reinterpret_cast<const char *>(hello), sizeof(hello));
        {
            SerialSRXL2 rejected(&tx, &rx, 14, 1);
            TEST_ASSERT_EQUAL(0, uartStartup1.begins);
            TEST_ASSERT_TRUE(crsfBatterySensorDetected);
            rejected.processSerialInput();
            TEST_ASSERT_EQUAL(sizeof(hello), rx.available());
            send(rejected, 110000);
            TEST_ASSERT_EQUAL(written, out.size());
            TEST_ASSERT_TRUE(crsfBatterySensorDetected);
        }
        TEST_ASSERT_TRUE(txDoneInterrupt == handler && txDoneArgument == argument);
        TEST_ASSERT_TRUE(crsfBatterySensorDetected);
        first.processSerialInput();
        TEST_ASSERT_EQUAL(0, rx.available());
        send(first, 111000);
        TEST_ASSERT_GREATER_THAN(written, out.size());
    }
    TEST_ASSERT_FALSE(crsfBatterySensorDetected);
    TEST_ASSERT_TRUE(txDoneInterrupt == nullptr);
    nowUs = 200000;
    SerialSRXL2 replacement(&tx, &rx, 14, 1);
    TEST_ASSERT_EQUAL(1, uartStartup1.begins);
    const size_t written = out.size();
    send(replacement, 250000);
    TEST_ASSERT_EQUAL(written + 14, out.size());
}

void test_failed_irq_allocation_is_inert_and_releases_its_receive_uart()
{
    std::string in(reinterpret_cast<const char *>(hello), sizeof(hello)), out;
    BinaryStringStream rx(in), tx(out);
    crsfBatterySensorDetected = true;
    irqAllocationSucceeds = false;
    SerialSRXL2 failed(&tx, &rx, 1);
    TEST_ASSERT_EQUAL(1, uartStartup.ends);
    failed.processSerialInput();
    send(failed, 50000);
    TEST_ASSERT_EQUAL(sizeof(hello), rx.available());
    TEST_ASSERT_TRUE(out.empty());
    TEST_ASSERT_TRUE(crsfBatterySensorDetected);
    irqAllocationSucceeds = true;
    SerialSRXL2 replacement(&tx, &rx, 1);
    replacement.processSerialInput();
    TEST_ASSERT_EQUAL(0, rx.available());
    send(replacement, 50174);
    TEST_ASSERT_EQUAL(14, out.size());
}

void test_airport_masks_only_the_primary_control_permission()
{
    firmwareOptions.is_airport = true;
    config.primary = PROTOCOL_CRSF;
    config.secondary = PROTOCOL_SERIAL1_SRXL2;
    std::string in, out;
    BinaryStringStream rx(in), tx(out);
    SerialSRXL2 driver(&tx, &rx, 14, 1);
    establish(driver, in, out);
    uint32_t channels[16] = {};
    channels[2] = 1811;
    nowUs = 63000;
    deliver(driver, true, channels);
    send(driver, 63000);
    TEST_ASSERT_EQUAL_HEX8(0xD5, uint8_t(out[out.size() - 3]));
}

void test_failed_receive_startup_is_inert_and_preserves_handoff_for_replacement()
{
    for (uint8_t port : {0, 1})
    {
        const int8_t pin = port == 0 ? 3 : 14;
        auto &uart = port == 0 ? uartStartup : uartStartup1;
        config.primary = port == 0 ? PROTOCOL_SRXL2 : PROTOCOL_CRSF;
        config.secondary = port == 1 ? PROTOCOL_SERIAL1_SRXL2 : PROTOCOL_SERIAL1_OFF;
        startupState = SRXL2StartupState();
        startupState.port = port;
        startupState.pin = pin;
        startupState.ackEndUs = 1000;
        nowUs = 2000;
        crsfBatterySensorDetected = true;
        uart.beginSucceeds = false;
        std::string in(reinterpret_cast<const char *>(hello), sizeof(hello)), out;
        BinaryStringStream rx(in), tx(out);
        SerialSRXL2 failed(&tx, &rx, pin, port);
        failed.processSerialInput();
        send(failed, 50000);
        TEST_ASSERT_EQUAL(sizeof(hello), rx.available());
        TEST_ASSERT_TRUE(out.empty());
        TEST_ASSERT_TRUE(txDoneInterrupt == nullptr);
        TEST_ASSERT_EQUAL(1, uart.ends);
        TEST_ASSERT_TRUE(crsfBatterySensorDetected);
        TEST_ASSERT_FALSE(startupState.handedOff);

        uart.beginSucceeds = true;
        nowUs = 55000;
        SerialSRXL2 replacement(&tx, &rx, pin, port);
        TEST_ASSERT_TRUE(startupState.handedOff);
        TEST_ASSERT_TRUE(txDoneInterrupt != nullptr);
        replacement.processSerialInput();
        TEST_ASSERT_EQUAL(0, rx.available());
        send(replacement, 55174);
        TEST_ASSERT_EQUAL(14, out.size());
        send(replacement, 56390);
    }
}

void run_adapter_tests()
{
    UnitySetTestFile(__FILE__);
    RUN_TEST(test_smart_startup_attaches_receive_without_driving_the_signal_pin);
    RUN_TEST(test_early_startup_ack_handles_a_fresh_announcement_and_releases_the_wire);
    RUN_TEST(test_early_startup_does_not_ack_corrupt_or_nonannouncement_frames);
    RUN_TEST(test_early_startup_acknowledges_buffered_announcements_after_hardware_idle);
    RUN_TEST(test_early_startup_recovers_after_unsuitable_input_before_an_announcement);
    RUN_TEST(test_normal_driver_finishes_acknowledged_startup_without_an_addressed_reply);
    RUN_TEST(test_adapter_ch3_and_all_inhibition_paths);
    RUN_TEST(test_adapter_missing_frames_do_not_refresh_cached_throttle);
    RUN_TEST(test_adapter_missed_flag_sends_fade_until_sample_expires);
    RUN_TEST(test_missing_raw_ch3_is_not_an_extreme_throttle_snapshot);
    RUN_TEST(test_adapter_real_crsf_payload_and_budget);
    RUN_TEST(test_esc_sensors_reach_the_real_elrs_downlink_queue);
    RUN_TEST(test_adapter_voltage_fallback_and_sentinel_suppression);
    RUN_TEST(test_rf_resync_drops_stale_callback_without_changing_core_latches);
    RUN_TEST(test_late_receive_hardware_and_partial_reply_block_transmit);
    RUN_TEST(test_first_pending_neutral_after_rf_reset_cannot_release_motion);
    RUN_TEST(test_live_driver_revokes_motion_when_another_protocol_is_selected);
    RUN_TEST(test_tx_interrupt_releases_bus_while_main_loop_is_suspended);
    RUN_TEST(test_split_echo_and_buffered_reply_survive_delayed_tx_done_callback);
    RUN_TEST(test_adapter_preserves_delayed_genuine_reply_after_complete_echo);
    RUN_TEST(test_secondary_uses_its_uart_and_ignores_primary_receive_activity);
    RUN_TEST(test_secondary_preserves_neutral_and_all_inhibition_paths);
    RUN_TEST(test_secondary_startup_fifo_prefix_and_handoff_match_port_and_pin);
    RUN_TEST(test_secondary_startup_rejects_a_corrupt_fifo_prefix);
    RUN_TEST(test_airport_masks_only_the_primary_control_permission);
    RUN_TEST(test_rejected_owner_leaves_uart_irq_input_and_telemetry_with_first_owner);
    RUN_TEST(test_failed_irq_allocation_is_inert_and_releases_its_receive_uart);
    RUN_TEST(test_failed_receive_startup_is_inert_and_preserves_handoff_for_replacement);
}

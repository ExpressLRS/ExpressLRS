#include <unity.h>
#include <string>
#include <vector>
#include "common.h"
#include "CRSFRouter.h"
#include "binary_serial.h"
using std::min;

connectionState_e connectionState = connected;
bool connectionHasModelMatch = true, teamraceHasModelMatch = true;
bool crsfBatterySensorDetected = false;
uint32_t ChannelData[CRSF_NUM_CHANNELS] = {};
CRSFRouter crsfRouter;
static bool uartReceiving = false;
static bool srxl2Selected = true;
static uint32_t nowUs;
static unsigned long testMicros() { return nowUs; }
static bool busDriving, txDrained, releasedBeforeDrain;
static bool autoTxDone = true;
static bool replyBlocked;
static unsigned diagnosticPublications;
static uint16_t signalEdges;
static UartStartupSpy uartStartup;
static constexpr uint32_t SERIAL_8N1 = 0x800001c;
static void (*txDoneInterrupt)(void *);
static void *txDoneArgument;
static bool installTxInterrupt(void (*handler)(void *), void *argument)
{
    txDoneInterrupt = handler;
    txDoneArgument = argument;
    return true;
}
static std::string *immediateReplyInput;
static std::string immediateReply;
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
#define SRXL2_DIAGNOSTICS
#define SRXL2_DIAGNOSTIC_PUBLISH() (++diagnosticPublications)
#define SRXL2_EDGE_COUNTER_INIT() true
#define SRXL2_EDGE_COUNT() signalEdges
#define SRXL2_EDGE_CLEAR() (signalEdges = 0)
#define SRXL2_HARDWARE_RX_BUSY() uartReceiving
#define SRXL2_MODE_ACTIVE() srxl2Selected
#define SRXL2_INSTALL_TX_IRQ(handler, argument) installTxInterrupt(handler, argument)
#define SRXL2_REMOVE_TX_IRQ() (txDoneInterrupt = nullptr)
#define SRXL2_BEGIN_TX() startBusTransmit()
#define SRXL2_POLL_TX_IRQ() do { if (autoTxDone) drainBusTransmit(); } while (0)
#define SRXL2_RELEASE_TX() releaseBusTransmit()
#define micros testMicros
#define Serial uartStartup
#include "../../src/rx-serial/SerialSRXL2.cpp"
#undef Serial
#undef micros
#undef SRXL2_HARDWARE_RX_BUSY
#undef SRXL2_MODE_ACTIVE
#undef SRXL2_BEGIN_TX
#undef SRXL2_POLL_TX_IRQ
#undef SRXL2_RELEASE_TX
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
    srxl2Selected = true;
    busDriving = txDrained = releasedBeforeDrain = false;
    autoTxDone = true;
    replyBlocked = false;
    diagnosticPublications = 0;
    signalEdges = 0;
    uartStartup = UartStartupSpy();
    txDoneInterrupt = nullptr;
    immediateReplyInput = nullptr;
    immediateReply.clear();
    for (auto &channel : ChannelData) channel = CRSF_CHANNEL_VALUE_UNSET;
    capture.addDevice(CRSF_ADDRESS_RADIO_TRANSMITTER);
    crsfRouter.addConnector(&capture);
}
void tearDown() { crsfRouter.removeConnector(&capture); }

static const uint8_t hello[] = {0xA6,0x21,14,0x40,0x21,10,0,0,0x11,0x22,0x33,0x44,0xC9,0xE7};
static const uint8_t esc[] = {0xA6,0x80,22,0x21,0x20,0,0x30,0x39,4,0xD2,1,0x5E,3,0xE8,0,0xFA,15,0x78,0x64,0x64,0x74,0x74};
static const uint8_t battery[] = {0xA6,0x80,22,0x21,0x42,0,0,0xF6,0xB8,0x0B,0,0,0x39,0x30,1,0x0E,0xD0,0x0E,0,0,0x97,0x9C};

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
    for (unsigned reason = 0; reason < 4; ++reason)
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
    srxl2Selected = false;
    send(driver, 73000);
    send(driver, 84500);
    assert_neutral(out);
    send(driver, 86000);
    srxl2Selected = true;
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

void test_diagnostics_publish_once_on_wifi_entry_without_transmitting()
{
    std::string in, out;
    BinaryStringStream rx(in), tx(out);
    SerialSRXL2 driver(&tx, &rx, 1);
    establish(driver, in, out);
    driver.event();
    TEST_ASSERT_EQUAL(0, diagnosticPublications);
    const size_t sent = out.size();
    connectionState = wifiUpdate;
    driver.event();
    TEST_ASSERT_EQUAL(1, diagnosticPublications);
    driver.event();
    TEST_ASSERT_EQUAL(1, diagnosticPublications);
    TEST_ASSERT_EQUAL(sent, out.size());
}

void test_wifi_live_capture_refreshes_while_motion_remains_inhibited()
{
    std::string in, out;
    BinaryStringStream rx(in), tx(out);
    SerialSRXL2 driver(&tx, &rx, 3);
    establish(driver, in, out);
    uint32_t channels[16] = {};
    channels[2] = 1811;
    nowUs = 63000;
    deliver(driver, true, channels);
    send(driver, 63000);
    send(driver, 64500);
    connectionState = wifiUpdate;
    driver.event();
    const unsigned entryPublications = diagnosticPublications;
    send(driver, 73000);
    send(driver, 84500);
    assert_neutral(out);
    input(driver, in, esc, sizeof(esc), 86500);
    send(driver, 1064500);
    assert_neutral(out);
    TEST_ASSERT_EQUAL(entryPublications + 1, diagnosticPublications);
    send(driver, 1064600);
    TEST_ASSERT_EQUAL(entryPublications + 1, diagnosticPublications);
    send(driver, 2064500);
    TEST_ASSERT_EQUAL(entryPublications + 2, diagnosticPublications);
}

void test_edge_capture_separates_transmit_from_undecodable_reply_activity()
{
    std::string in, out;
    BinaryStringStream rx(in), tx(out);
    SerialSRXL2 driver(&tx, &rx, 3);
    signalEdges = 7; // Digital activity before the first request, no UART bytes.
    send(driver, 50000);
    signalEdges = 36; // Rising edges independently counted in the startup vector.
    send(driver, 51216);
    const auto &state = driver.getDiagnostics();
    TEST_ASSERT_TRUE(state.edgeCounterReady);
    TEST_ASSERT_EQUAL(7, state.rxWireEdges);
    TEST_ASSERT_EQUAL(36, state.txWireEdges);
    TEST_ASSERT_EQUAL(36, state.txExpectedEdges);
    signalEdges = 11; // A malformed reply is still visible to the pulse counter.
    send(driver, 100000);
    TEST_ASSERT_EQUAL(18, state.rxWireEdges);
    TEST_ASSERT_EQUAL(36, state.txWireEdges);
    TEST_ASSERT_EQUAL(0, state.rxBytes);
}

void test_wifi_probe_changes_only_the_discovery_destination()
{
    TEST_ASSERT_FALSE(setSRXL2ProbeAddress(0x41)); // No probe overrides in RF operation.
    connectionState = wifiUpdate;
    TEST_ASSERT_FALSE(setSRXL2ProbeAddress(0x3F));
    TEST_ASSERT_FALSE(setSRXL2ProbeAddress(0x50));
    TEST_ASSERT_TRUE(setSRXL2ProbeAddress(0x4F));
    std::string in, out;
    BinaryStringStream rx(in), tx(out);
    SerialSRXL2 driver(&tx, &rx, 3);
    send(driver, 50000);
    const uint8_t expected[] = {0xA6,0x21,0x0E,0x21,0x4F,0x0A,0,3,0x78,0x56,0x34,0x12,0x2A,0x53};
    TEST_ASSERT_EQUAL(sizeof(expected), out.size());
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, reinterpret_cast<const uint8_t *>(out.data()), sizeof(expected));
    send(driver, 51216);
    connectionState = connected;
    send(driver, 100000);
    TEST_ASSERT_EQUAL(28, out.size());
    TEST_ASSERT_EQUAL_HEX8(0x40, uint8_t(out[18])); // Probe setting cannot affect RF operation.
    connectionState = wifiUpdate;
    TEST_ASSERT_TRUE(setSRXL2ProbeAddress(0x40));
}

void test_startup_capture_keeps_early_bytes_after_the_tail_is_overwritten()
{
    std::string in, out;
    BinaryStringStream rx(in), tx(out);
    SerialSRXL2 driver(&tx, &rx, 3);
    uint8_t early[126], later[200];
    std::memset(early, 0x55, sizeof(early));
    std::memset(later, 0x5A, sizeof(later));
    input(driver, in, early, sizeof(early), 12345);
    while (rx.available()) driver.processSerialInput();
    send(driver, 50000);
    send(driver, 51216);
    input(driver, in, later, sizeof(later), 60000);
    while (rx.available()) driver.processSerialInput();
    const auto &state = driver.getDiagnostics();
    TEST_ASSERT_EQUAL(12345, state.firstRxUs);
    TEST_ASSERT_EQUAL(126, state.rxBeforeFirstTx);
    TEST_ASSERT_EQUAL(256, state.rxHeadSize);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(early, state.rxHead, sizeof(early));
    for (unsigned i = sizeof(early); i < sizeof(state.rxHead); ++i)
        TEST_ASSERT_EQUAL_HEX8(0x5A, state.rxHead[i]);
    for (auto byte : state.rxTail) TEST_ASSERT_EQUAL_HEX8(0x5A, byte);
}

void test_diagnostics_keep_rf_commands_and_received_bytes_for_wifi_capture()
{
    std::string in, out;
    BinaryStringStream rx(in), tx(out);
    SerialSRXL2 driver(&tx, &rx, 1);
    establish(driver, in, out);
    const auto &state = driver.getDiagnostics();
    TEST_ASSERT_EQUAL(2, state.frames);
    TEST_ASSERT_EQUAL(53000, state.firstRFFrameUs);
    TEST_ASSERT_EQUAL(3, state.txPackets);
    TEST_ASSERT_EQUAL(3, state.txDone);
    TEST_ASSERT_EQUAL(sizeof(hello) + sizeof(esc), state.rxBytes);
    TEST_ASSERT_EQUAL(992, state.ch3);
    TEST_ASSERT_EQUAL(1, state.lastRfTx[3]); // Neutral gate hadn't been released at TX.

    uint32_t channels[16] = {};
    channels[2] = 1811;
    nowUs = 63000;
    deliver(driver, true, channels);
    send(driver, 63000);
    TEST_ASSERT_TRUE(state.allowed);
    TEST_ASSERT_EQUAL(992, state.ch3Min);
    TEST_ASSERT_EQUAL(1811, state.ch3Max);
    TEST_ASSERT_EQUAL(0, state.lastRfTx[3]);
    TEST_ASSERT_EQUAL_HEX8(0x54, state.lastRfTx[12]);
    TEST_ASSERT_EQUAL_HEX8(0xD5, state.lastRfTx[13]);
    TEST_ASSERT_EQUAL(1, state.normalTxPackets);
    TEST_ASSERT_EQUAL(63000, state.firstNormalTxUs);
    TEST_ASSERT_EQUAL(16, state.lastNormalTxLength);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(state.lastRfTx, state.lastNormalTx, 16);
    TEST_ASSERT_EQUAL(55000, state.lastTelemetryUs);

    uint8_t bytes[80];
    for (unsigned i = 0; i < sizeof(bytes); ++i) bytes[i] = i;
    input(driver, in, bytes, sizeof(bytes), 65000);
    driver.processSerialInput(); // The existing read limit is 64 bytes per call.
    for (unsigned i = 0; i < 64; ++i)
        TEST_ASSERT_EQUAL(i + 16, state.rxTail[(state.rxBytes % 64 + i) % 64]);
    input(driver, in, esc, sizeof(esc), 66000);
    nowUs = 83000;
    deliver(driver, true, channels);
    send(driver, 83000);
    TEST_ASSERT_EQUAL(2, state.normalTxPackets);
    TEST_ASSERT_EQUAL(20000, state.normalTxSpacingMinUs);
    TEST_ASSERT_EQUAL(66000, state.lastTelemetryUs);
    connectionState = wifiUpdate;
    ChannelData[2] = CRSF_CHANNEL_VALUE_UNSET;
    driver.sendRCFrame(false, false, channels);
    driver.event();
    TEST_ASSERT_TRUE(state.rfConnected);
    TEST_ASSERT_TRUE(state.allowed);
    TEST_ASSERT_EQUAL(1811, state.ch3);
    TEST_ASSERT_EQUAL(1, diagnosticPublications);
}

void test_diagnostics_trace_first_handshake_before_the_rf_link_connects()
{
    connectionState = disconnected;
    nowUs = 1000;
    std::string in, out;
    BinaryStringStream rx(in), tx(out);
    SerialSRXL2 driver(&tx, &rx, 1);
    const auto &state = driver.getDiagnostics();
    TEST_ASSERT_EQUAL(1000, state.driverInitUs);
    send(driver, 50999);
    TEST_ASSERT_TRUE(out.empty());
    send(driver, 51000);
    TEST_ASSERT_EQUAL(14, out.size());
    TEST_ASSERT_EQUAL(51000, state.firstTxUs);
    send(driver, 52216); // Complete the 14-byte transmission before the next poll.
    send(driver, 101000);
    TEST_ASSERT_EQUAL(28, out.size());
    TEST_ASSERT_EQUAL(51000, state.firstTxUs);
}

void test_diagnostics_measure_tx_completion_delay_without_changing_packets()
{
    std::string in, out;
    BinaryStringStream rx(in), tx(out);
    SerialSRXL2 driver(&tx, &rx, 1);
    const auto &state = driver.getDiagnostics();
    send(driver, 50000);
    TEST_ASSERT_EQUAL(14, out.size());
    send(driver, 51216); // 14 UART bytes at 115200 baud take 1216 us, rounded up.
    TEST_ASSERT_EQUAL(1216, state.txDurationMaxUs);
    TEST_ASSERT_EQUAL(0, state.txDelayMaxUs);
    TEST_ASSERT_EQUAL(0, state.txDelayMinUs);
    TEST_ASSERT_EQUAL(0, state.txDelayLongCount);
    send(driver, 100000);
    TEST_ASSERT_EQUAL(28, out.size());
    send(driver, 102216); // Simulate the completion IRQ being delayed by 1000 us.
    TEST_ASSERT_EQUAL(2216, state.txDurationMaxUs);
    TEST_ASSERT_EQUAL(1000, state.txDelayMaxUs);
    TEST_ASSERT_EQUAL(0, state.txDelayMinUs);
    TEST_ASSERT_EQUAL(1, state.txDelayLongCount);
    TEST_ASSERT_EQUAL(2, state.txDone);
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_smart_startup_attaches_receive_without_driving_the_signal_pin);
    RUN_TEST(test_adapter_ch3_and_all_inhibition_paths);
    RUN_TEST(test_adapter_missing_frames_do_not_refresh_cached_throttle);
    RUN_TEST(test_adapter_missed_flag_sends_fade_until_sample_expires);
    RUN_TEST(test_missing_raw_ch3_is_not_an_extreme_throttle_snapshot);
    RUN_TEST(test_adapter_real_crsf_payload_and_budget);
    RUN_TEST(test_adapter_voltage_fallback_and_sentinel_suppression);
    RUN_TEST(test_rf_resync_drops_stale_callback_without_changing_core_latches);
    RUN_TEST(test_late_receive_hardware_and_partial_reply_block_transmit);
    RUN_TEST(test_first_pending_neutral_after_rf_reset_cannot_release_motion);
    RUN_TEST(test_live_driver_revokes_motion_when_another_protocol_is_selected);
    RUN_TEST(test_tx_interrupt_releases_bus_while_main_loop_is_suspended);
    RUN_TEST(test_split_echo_and_buffered_reply_survive_delayed_tx_done_callback);
    RUN_TEST(test_adapter_preserves_delayed_genuine_reply_after_complete_echo);
    RUN_TEST(test_diagnostics_publish_once_on_wifi_entry_without_transmitting);
    RUN_TEST(test_wifi_live_capture_refreshes_while_motion_remains_inhibited);
    RUN_TEST(test_edge_capture_separates_transmit_from_undecodable_reply_activity);
    RUN_TEST(test_wifi_probe_changes_only_the_discovery_destination);
    RUN_TEST(test_startup_capture_keeps_early_bytes_after_the_tail_is_overwritten);
    RUN_TEST(test_diagnostics_keep_rf_commands_and_received_bytes_for_wifi_capture);
    RUN_TEST(test_diagnostics_trace_first_handshake_before_the_rf_link_connects);
    RUN_TEST(test_diagnostics_measure_tx_completion_delay_without_changing_packets);
    return UNITY_END();
}

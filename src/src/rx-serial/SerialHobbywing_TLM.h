#pragma once

#include "SerialIO.h"
#include "device.h"

#include <cstddef>

#if defined(TARGET_RX) || defined(UNIT_TEST)

class SerialHobbywingTlmTestAccess;

// Receive-only bridge for unsolicited Hobbywing ESC telemetry.
// The ESC speaks either the V4 protocol at 19200 baud or the Platinum V5 protocol
// at 115200 baud. Both are probed in turn until valid frames confirm one of them,
// which is then kept until power cycle.
// Valid ESC frames are translated to standard CRSF telemetry frames.
class SerialHobbywing_TLM final : public SerialIO
{
public:
    explicit SerialHobbywing_TLM(HardwareSerial &port) : SerialIO(&port, &port), port(port) {}
    ~SerialHobbywing_TLM() override = default;

    uint32_t sendRCFrame(bool frameAvailable, bool frameMissed, uint32_t *channelData) override { return DURATION_IMMEDIATELY; }

private:
    friend class SerialHobbywingTlmTestAccess;

    static constexpr uint32_t V4_BAUD = 19200;
    static constexpr uint8_t V4_SYNC_BYTE = 0x9B;
    static constexpr uint8_t V4_INFO_FRAME_LENGTH = 13;
    static constexpr uint8_t V4_DATA_FRAME_LENGTH = 19;
    static constexpr uint8_t V4_TRAILING_MARKER = 0xB9;

    static constexpr uint32_t V5_BAUD = 115200;
    static constexpr uint8_t V5_SYNC_BYTE = 0xFE;
    static constexpr uint8_t V5_FRAME_LENGTH = 32;
    static constexpr uint8_t V5_CRC_OFFSET = 30;
    static_assert(V5_CRC_OFFSET + sizeof(uint16_t) == V5_FRAME_LENGTH, "Invalid Hobbywing V5 frame layout");

    // The other protocol is tried after this long without a valid frame. Both
    // protocols drop to 2.5 frames per second at idle. A V4 INFO frame confirms
    // the protocol on its own. V4 data frames carry no checksum, so more of them
    // are needed than CRC-checked V5 frames.
    static constexpr uint32_t PROBE_DWELL_MS = 1500;
    static constexpr uint8_t V4_INFO_CONFIRM_FRAMES = 1;
    static constexpr uint8_t V4_DATA_CONFIRM_FRAMES = 3;
    static constexpr uint8_t V5_CONFIRM_FRAMES = 2;
    static constexpr uint32_t PARTIAL_FRAME_TIMEOUT_MS = 50;

    static constexpr uint8_t RPM_SOURCE_ID = 0;           // Motor 1
    static constexpr uint8_t TEMPERATURE_SOURCE_ID = 0;   // ESC FET
    static constexpr uint8_t BEC_VOLTAGE_SOURCE_ID = 129; // Volt sensor 1

    struct CalibrationData
    {
        uint8_t voltageNumerator = 0;   // 0.1 volt per voltage ADC count = num / den
        uint8_t voltageDenominator = 0;
        uint8_t currentNumerator = 0;   // ampere per current ADC count = num / den
        uint8_t currentDenominator = 0;
        uint8_t currentOffset = 0;      // current zero offset in ampere
    };

    struct DecodedData
    {
        uint32_t rpm = 0;                   // electrical revolutions per minute
        uint16_t voltageDeciVolts = 0;      // 0.1 volt
        uint16_t currentDeciAmps = 0;       // 0.1 ampere
        int16_t temperatureDeciCelsius = 0; // 0.1 degree Celsius
        uint16_t becMillivolts = 0;         // millivolt
        bool hasBattery = false;            // voltage and current are valid
        bool hasTemperature = false;        // temperature is valid
        bool hasBecVoltage = false;         // BEC voltage is valid
    };

    void processBytes(uint8_t *bytes, uint16_t size) override;
    void processByteV4(uint8_t value);
    void processByteV5(uint8_t value);
    void resetParser();
    void resynchronizeParser(uint8_t syncByte);
    void switchProtocol(uint32_t now);
    bool confirmProtocol(uint8_t requiredFrames);

    bool isInfoFrameV4() const;
    bool validateDataFrameV4() const;
    bool validateFrameV5() const;
    void applyInfoFrameV4();
    bool decodeDataFrameV4();
    void decodeFrameV5();
    void scheduleTelemetry();

    void sendCRSFbattery();
    void sendCRSFrpm();
    void sendCRSFtemp();
    void sendCRSFbecVoltage();

    static uint16_t readU16BE(const uint8_t *data);
    static uint32_t readU24BE(const uint8_t *data);
    static uint16_t readU16LE(const uint8_t *data);
    static uint16_t calculateCrc(const uint8_t *data, size_t length);
    static bool partialFrameTimedOut(uint32_t now, uint32_t lastReceived);
    static bool decodeTemperatureDeciCelsius(uint16_t adc, int16_t &outDeciCelsius);

    HardwareSerial &port;

    // Sized for the V5 frame, the longer of the two.
    uint8_t frame[V5_FRAME_LENGTH] = {};
    uint8_t framePosition = 0;

    // V4 is probed first: its INFO frames carry the calibration and are only
    // sent until the throttle is first raised.
    bool protocolV5 = false;
    bool protocolConfirmed = false;
    uint8_t probeFrames = 0;
    uint32_t probeStartMs = 0;

    uint32_t lastReceivedByteMs = 0;
    uint32_t validFrameCount = 0;
    bool sendTemperatureNext = true;

    CalibrationData calibration;
    DecodedData decoded;
};

#endif

#pragma once

#include "SerialIO.h"
#include "device.h"

#include <cstddef>

#if defined(TARGET_RX) || defined(UNIT_TEST)

class SerialScorpionTlmTestAccess;

// Receive-only bridge for unsolicited Scorpion Tribunus telemetry.
// Valid ESC frames are translated to standard CRSF telemetry frames.
class SerialScorpion_TLM final : public SerialIO
{
public:
    SerialScorpion_TLM(Stream &outputPort, Stream &inputPort);
    ~SerialScorpion_TLM() override = default;

    uint32_t sendRCFrame(bool frameAvailable, bool frameMissed, uint32_t *channelData) override;
    void sendQueuedData(uint32_t maxBytesToSend) override;

private:
    friend class SerialScorpionTlmTestAccess;

    static constexpr uint8_t PACKET_TYPE = 0x55;
    static constexpr uint8_t FRAME_LENGTH = 22;
    static constexpr uint8_t LENGTH_OFFSET = 2;
    static constexpr uint8_t CRC_OFFSET = 20;
    static constexpr uint32_t PARTIAL_FRAME_TIMEOUT_MS = 50;
    static_assert(CRC_OFFSET + sizeof(uint16_t) == FRAME_LENGTH, "Invalid Scorpion frame layout");

    static constexpr uint8_t RPM_SOURCE_ID = 0;           // Motor 1
    static constexpr uint8_t TEMPERATURE_SOURCE_ID = 0;   // FC / all ESCs
    static constexpr uint8_t BEC_VOLTAGE_SOURCE_ID = 129; // Volt sensor 1

    struct DecodedData
    {
        uint32_t timestampMs = 0;                  // ESC timestamp in milliseconds
        uint16_t throttlePermille = 0;             // 0.1 percent
        uint16_t currentDeciAmps = 0;              // 0.1 ampere
        uint16_t voltageDeciVolts = 0;             // 0.1 volt
        uint16_t consumedMah = 0;                  // milliampere-hour
        uint16_t temperatureDeciCelsius = 0;       // 0.1 degree Celsius
        uint16_t pwmPermille = 0;                  // 0.1 percent
        uint16_t becMillivolts = 0;                // millivolt
        uint32_t rpm = 0;                          // revolutions per minute
        uint8_t status = 0;                        // ESC status value
    };

    void processBytes(uint8_t *bytes, uint16_t size) override;
    void resetParser();
    void resynchronizeParser();
    void processByte(uint8_t value);
    bool validateFrame() const;
    void decodeFrame();
    void scheduleTelemetry();

    void sendCRSFbattery();
    void sendCRSFrpm();
    void sendCRSFtemp();
    void sendCRSFbecVoltage();

    static uint16_t readU16LE(const uint8_t *data);
    static uint32_t readU24LE(const uint8_t *data);
    static uint16_t calculateCrc(const uint8_t *data, size_t length);
    static bool partialFrameTimedOut(uint32_t now, uint32_t lastReceived);

    uint8_t frame[FRAME_LENGTH] = {};
    uint8_t framePosition = 0;

    uint32_t lastReceivedByteMs = 0;
    uint32_t validFrameCount = 0;
    bool sendTemperatureNext = true;

    DecodedData decoded;
};

#endif

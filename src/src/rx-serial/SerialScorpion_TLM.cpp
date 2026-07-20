#include "SerialScorpion_TLM.h"

#if defined(TARGET_RX) || defined(UNIT_TEST)

#include "CRSFRouter.h"
#include "common.h"
#include "crsf_protocol.h"

#include <cstring>

namespace
{
constexpr uint8_t SIZE_8BIT = 1;
constexpr uint8_t SIZE_16BIT = 2;
constexpr uint8_t SIZE_24BIT = 3;

// Scorpion telemetry is an unsolicited 22-byte, little-endian frame at 38400 8N1.
// Only fields consumed by this driver are listed below:
//   0       packet type (0x55)
//   2       frame length (22)
//   4..6    timestamp (ms)
//   7       throttle (0.5%)
//   8..9    current (0.1A)
//   10..11  pack voltage (0.1V)
//   12..13  consumed capacity (mAh)
//   14      ESC temperature (1 degree C)
//   15      PWM (0.5%)
//   16      BEC voltage (0.1V)
//   17..18  RPM divided by 5
//   19      status
//   20..21  CRC16, least-significant byte first
constexpr uint8_t TIMESTAMP_OFFSET = 4;
constexpr uint8_t THROTTLE_OFFSET = 7;
constexpr uint8_t CURRENT_OFFSET = 8;
constexpr uint8_t VOLTAGE_OFFSET = 10;
constexpr uint8_t CAPACITY_OFFSET = 12;
constexpr uint8_t TEMPERATURE_OFFSET = 14;
constexpr uint8_t PWM_OFFSET = 15;
constexpr uint8_t BEC_VOLTAGE_OFFSET = 16;
constexpr uint8_t RPM_OFFSET = 17;
constexpr uint8_t STATUS_OFFSET = 19;

constexpr uint16_t THROTTLE_SCALE = 5;
constexpr uint16_t TEMPERATURE_SCALE = 10;
constexpr uint16_t PWM_SCALE = 5;
constexpr uint16_t BEC_VOLTAGE_SCALE = 100;
constexpr uint16_t RPM_SCALE = 5;

constexpr uint8_t SLOW_TELEMETRY_INTERVAL = 5;
constexpr uint16_t CRC_POLYNOMIAL = 0x8408;
}

SerialScorpion_TLM::SerialScorpion_TLM(Stream &outputPort, Stream &inputPort)
        : SerialIO(&outputPort, &inputPort)
{
}

uint32_t SerialScorpion_TLM::sendRCFrame(bool frameAvailable, bool frameMissed, uint32_t *channelData)
{
    (void)frameAvailable;
    (void)frameMissed;
    (void)channelData;
    return DURATION_IMMEDIATELY;
}

void SerialScorpion_TLM::sendQueuedData(uint32_t maxBytesToSend)
{
    (void)maxBytesToSend;
    const uint32_t now = millis();
    if (framePosition != 0 && partialFrameTimedOut(now, lastReceivedByteMs))
    {
        resetParser();
    }
}

void SerialScorpion_TLM::processBytes(uint8_t *bytes, uint16_t size)
{
    for (uint16_t i = 0; i < size; ++i)
    {
        processByte(bytes[i]);
    }
}

void SerialScorpion_TLM::resetParser()
{
    framePosition = 0;
}

void SerialScorpion_TLM::resynchronizeParser()
{
    // Retain the longest suffix that may be the start of the next frame.
    while (framePosition != 0)
    {
        uint8_t syncPosition = 1;
        while (syncPosition < framePosition && frame[syncPosition] != PACKET_TYPE)
        {
            ++syncPosition;
        }

        if (syncPosition == framePosition)
        {
            resetParser();
            return;
        }

        framePosition -= syncPosition;
        std::memmove(frame, frame + syncPosition, framePosition);

        if (framePosition <= LENGTH_OFFSET || frame[LENGTH_OFFSET] == FRAME_LENGTH)
        {
            return;
        }
    }
}

void SerialScorpion_TLM::processByte(uint8_t value)
{
    const uint32_t now = millis();

    // Discard an incomplete frame after a gap in the serial stream.
    if (framePosition != 0 && partialFrameTimedOut(now, lastReceivedByteMs))
    {
        resetParser();
    }

    if (framePosition == 0)
    {
        if (value != PACKET_TYPE)
        {
            return;
        }

        frame[framePosition++] = value;
        lastReceivedByteMs = now;
        return;
    }

    frame[framePosition++] = value;
    lastReceivedByteMs = now;

    if (framePosition == LENGTH_OFFSET + 1 && frame[LENGTH_OFFSET] != FRAME_LENGTH)
    {
        resynchronizeParser();
        return;
    }

    if (framePosition != FRAME_LENGTH)
    {
        return;
    }

    if (validateFrame())
    {
        decodeFrame();
        scheduleTelemetry();
        resetParser();
    }
    else
    {
        resynchronizeParser();
    }
}

bool SerialScorpion_TLM::validateFrame() const
{
    if (frame[0] != PACKET_TYPE || frame[LENGTH_OFFSET] != FRAME_LENGTH)
    {
        return false;
    }

    const uint16_t receivedCrc = readU16LE(frame + CRC_OFFSET);
    return calculateCrc(frame, CRC_OFFSET) == receivedCrc;
}

void SerialScorpion_TLM::decodeFrame()
{
    decoded.timestampMs = readU24LE(frame + TIMESTAMP_OFFSET);
    decoded.throttlePermille = static_cast<uint16_t>(frame[THROTTLE_OFFSET]) * THROTTLE_SCALE;
    decoded.currentDeciAmps = readU16LE(frame + CURRENT_OFFSET);
    decoded.voltageDeciVolts = readU16LE(frame + VOLTAGE_OFFSET);
    decoded.consumedMah = readU16LE(frame + CAPACITY_OFFSET);
    decoded.temperatureDeciCelsius = static_cast<uint16_t>(frame[TEMPERATURE_OFFSET]) * TEMPERATURE_SCALE;
    decoded.pwmPermille = static_cast<uint16_t>(frame[PWM_OFFSET]) * PWM_SCALE;
    decoded.becMillivolts = static_cast<uint16_t>(frame[BEC_VOLTAGE_OFFSET]) * BEC_VOLTAGE_SCALE;
    decoded.rpm = static_cast<uint32_t>(readU16LE(frame + RPM_OFFSET)) * RPM_SCALE;
    decoded.status = frame[STATUS_OFFSET];
}

void SerialScorpion_TLM::scheduleTelemetry()
{
    ++validFrameCount;

    // Battery and RPM are sent on alternating frames. Temperature and BEC voltage
    // are interleaved at a lower rate to avoid consuming the telemetry link.
    if ((validFrameCount & 1U) != 0)
    {
        sendCRSFbattery();
    }
    else
    {
        sendCRSFrpm();
    }

    if (validFrameCount % SLOW_TELEMETRY_INTERVAL == 0)
    {
        if (sendTemperatureNext)
        {
            sendCRSFtemp();
        }
        else
        {
            sendCRSFbecVoltage();
        }
        sendTemperatureNext = !sendTemperatureNext;
    }
}

void SerialScorpion_TLM::sendCRSFbattery()
{
    crsfBatterySensorDetected = true;

    // Pack voltage, current and consumed capacity share the standard CRSF battery sensor.
    CRSF_MK_FRAME_T(crsf_sensor_battery_t) crsfBattery = {0};
    crsfBattery.p.voltage = htobe16(decoded.voltageDeciVolts);
    crsfBattery.p.current = htobe16(decoded.currentDeciAmps);
    crsfBattery.p.capacity = htobe24(decoded.consumedMah);
    crsfBattery.p.remaining = 0;

    crsfRouter.SetHeaderAndCrc(&crsfBattery.h, CRSF_FRAMETYPE_BATTERY_SENSOR,
        CRSF_FRAME_SIZE(sizeof(crsf_sensor_battery_t)));
    crsfRouter.deliverMessageTo(CRSF_ADDRESS_RADIO_TRANSMITTER, &crsfBattery.h);
}

void SerialScorpion_TLM::sendCRSFrpm()
{
    CRSF_MK_FRAME_T(crsf_sensor_rpm_t) crsfRpm = {0};
    crsfRpm.p.source_id = RPM_SOURCE_ID;
    crsfRpm.p.rpm0 = htobe24(decoded.rpm);

    constexpr uint8_t payloadSize = SIZE_8BIT + SIZE_24BIT;
    crsfRouter.SetHeaderAndCrc(&crsfRpm.h, CRSF_FRAMETYPE_RPM, CRSF_FRAME_SIZE(payloadSize));
    crsfRouter.deliverMessageTo(CRSF_ADDRESS_RADIO_TRANSMITTER, &crsfRpm.h);
}

void SerialScorpion_TLM::sendCRSFtemp()
{
    CRSF_MK_FRAME_T(crsf_sensor_temp_t) crsfTemperature = {0};
    crsfTemperature.p.source_id = TEMPERATURE_SOURCE_ID;
    crsfTemperature.p.temperature[0] = htobe16(decoded.temperatureDeciCelsius);

    constexpr uint8_t payloadSize = SIZE_8BIT + SIZE_16BIT;
    crsfRouter.SetHeaderAndCrc(&crsfTemperature.h, CRSF_FRAMETYPE_TEMP, CRSF_FRAME_SIZE(payloadSize));
    crsfRouter.deliverMessageTo(CRSF_ADDRESS_RADIO_TRANSMITTER, &crsfTemperature.h);
}

void SerialScorpion_TLM::sendCRSFbecVoltage()
{
    // Source IDs 128 and above are shown as independent Volt sensors by EdgeTX.
    CRSF_MK_FRAME_T(crsf_sensor_cells_t) crsfBecVoltage = {0};
    crsfBecVoltage.p.source_id = BEC_VOLTAGE_SOURCE_ID;
    crsfBecVoltage.p.cell[0] = htobe16(decoded.becMillivolts);

    constexpr uint8_t payloadSize = SIZE_8BIT + SIZE_16BIT;
    crsfRouter.SetHeaderAndCrc(&crsfBecVoltage.h, CRSF_FRAMETYPE_CELLS, CRSF_FRAME_SIZE(payloadSize));
    crsfRouter.deliverMessageTo(CRSF_ADDRESS_RADIO_TRANSMITTER, &crsfBecVoltage.h);
}

uint16_t SerialScorpion_TLM::readU16LE(const uint8_t *data)
{
    return static_cast<uint16_t>(data[0]) |
        static_cast<uint16_t>(data[1]) << 8U;
}

uint32_t SerialScorpion_TLM::readU24LE(const uint8_t *data)
{
    return static_cast<uint32_t>(data[0]) |
        static_cast<uint32_t>(data[1]) << 8U |
        static_cast<uint32_t>(data[2]) << 16U;
}

uint16_t SerialScorpion_TLM::calculateCrc(const uint8_t *data, size_t length)
{
    // Reflected CRC16, polynomial 0x8408, initial value 0. The protocol layout and
    // CRC parameters were referenced from Rotorflight's GPLv3 Scorpion implementation:
    // https://github.com/rotorflight/rotorflight-firmware/blob/master/src/main/sensors/esc_sensor.c
    uint16_t crc = 0;

    while (length-- != 0)
    {
        crc ^= *data++;

        for (uint8_t bit = 0; bit < 8; ++bit)
        {
            crc = (crc & 1U) != 0
                ? static_cast<uint16_t>((crc >> 1U) ^ CRC_POLYNOMIAL)
                : static_cast<uint16_t>(crc >> 1U);
        }
    }

    return crc;
}

bool SerialScorpion_TLM::partialFrameTimedOut(uint32_t now, uint32_t lastReceived)
{
    return static_cast<uint32_t>(now - lastReceived) >= PARTIAL_FRAME_TIMEOUT_MS;
}

#endif

#include "SerialHobbywing_TLM.h"

#if defined(TARGET_RX) || defined(UNIT_TEST)

#include "CRSFRouter.h"
#include "common.h"
#include "crsf_protocol.h"
#include "logging.h"

#include <cstring>

namespace
{
constexpr uint8_t SIZE_8BIT = 1;
constexpr uint8_t SIZE_16BIT = 2;
constexpr uint8_t SIZE_24BIT = 3;

// Hobbywing V4 telemetry is an unsolicited, big-endian stream at 19200 8N1. The ESC
// sends a 13-byte INFO frame carrying calibration constants and 19-byte DATA frames.
// Only fields consumed by this driver are listed below.
//
// DATA frame (19 bytes):
//   0       sync (0x9B)
//   4..5    throttle (raw)
//   6..7    PWM output (raw, plausibility check only)
//   8..10   eRPM
//   11..12  voltage ADC
//   13..14  current ADC
//   15..16  FET temperature ADC
//   17..18  capacitor temperature ADC (plausibility check only)
constexpr uint8_t V4_THROTTLE_OFFSET = 4;
constexpr uint8_t V4_PWM_OFFSET = 6;
constexpr uint8_t V4_RPM_OFFSET = 8;
constexpr uint8_t V4_VOLTAGE_ADC_OFFSET = 11;
constexpr uint8_t V4_CURRENT_ADC_OFFSET = 13;
constexpr uint8_t V4_FET_TEMP_ADC_OFFSET = 15;
constexpr uint8_t V4_CAP_TEMP_ADC_OFFSET = 17;

// INFO frame (13 bytes):
//   0..1    sync (0x9B 0x9B)
//   4       RPM steps (always 1)
//   5..6    voltage scale numerator, denominator
//   7..9    current scale numerator, denominator, offset
//   12      trailing marker (0xB9)
constexpr uint8_t V4_INFO_RPM_STEPS_OFFSET = 4;
constexpr uint8_t V4_INFO_VOLTAGE_OFFSET = 5;
constexpr uint8_t V4_INFO_CURRENT_OFFSET = 7;

// Data frames carry no checksum, so plausibility limits on the high bytes of the raw
// fields stand in for one: throttle and PWM stay below 1024, ADC readings below 4096
// (12 bit), and the RPM has to fit the signed 24-bit CRSF rpm0 field.
constexpr uint8_t V4_THROTTLE_HIGH_BYTE_LIMIT = 0x04;
constexpr uint8_t V4_ADC_HIGH_BYTE_LIMIT = 0x10;
constexpr uint8_t V4_RPM_HIGH_BYTE_LIMIT = 0x80;
constexpr uint32_t V4_DECI_AMPS_PER_AMP = 10;
constexpr uint32_t V4_MAX_VOLTAGE_DECI_VOLTS = 1000;
constexpr uint32_t V4_MAX_CURRENT_DECI_AMPS = 5000;

constexpr uint16_t TEMPERATURE_TABLE_STEP = 64;

// The NTC curve is too steep to interpolate in the first and the last table interval,
// so readings beyond the second and the second to last entry are not reported.
constexpr int16_t MIN_TEMPERATURE_DECI_CELSIUS = -238;
constexpr int16_t MAX_TEMPERATURE_DECI_CELSIUS = 2495;

// V4 FET temperature in 0.1 degree Celsius at every 64th ADC count, precomputed from
// the Steinhart-Hart NTC coefficients (gamma 0.00025316455696, delta 0.00296226896087)
// referenced from Rotorflight's GPLv3 Hobbywing implementation:
// https://github.com/rotorflight/rotorflight-firmware/blob/master/src/main/sensors/esc_sensor.c
// The table is kept coarse to save flash: an entry at every 32nd count would be accurate
// to 0.2 degrees but costs 128 bytes more. Linear interpolation of this table stays
// within 0.2 degrees of the closed form from 0 to 120 C, within 1 degree from -20 to
// 150 C and within 5 degrees up to the 249 C limit.
// PROGMEM keeps the table out of ESP8285 RAM.
constexpr int16_t TEMPERATURE_DECI_CELSIUS_TABLE[] PROGMEM = {
    8943, 2495, 2047, 1815, 1661, 1547, 1456, 1381,
    1318, 1262, 1213, 1168, 1128, 1091, 1056, 1024,
    994, 966, 939, 913, 888, 865, 842, 820,
    798, 778, 757, 738, 718, 699, 681, 662,
    644, 626, 609, 591, 573, 556, 538, 521,
    503, 485, 468, 449, 431, 413, 394, 374,
    355, 334, 313, 291, 268, 244, 219, 192,
    163, 131, 96, 56, 10, -47, -122, -238,
    -758
};
static_assert(sizeof(TEMPERATURE_DECI_CELSIUS_TABLE) / sizeof(TEMPERATURE_DECI_CELSIUS_TABLE[0]) ==
    4096 / TEMPERATURE_TABLE_STEP + 1, "Temperature table must cover the full 12-bit ADC range");

// Hobbywing Platinum V5 telemetry is an unsolicited, little-endian stream at 115200 8N1.
// One 32-byte frame: 6 header bytes, a length byte, 23 data bytes and a CRC16.
// Only fields consumed by this driver are listed below.
//   0       sync (0xFE)
//   3       protocol version (3)
//   4..5    frame type (0x5C30, least-significant byte first)
//   9       throttle (1 percent)
//   13..14  eRPM divided by 10
//   15..16  ESC voltage (0.1V)
//   17..18  ESC current (0.1A)
//   19      FET temperature (1 degree C)
//   22      BEC voltage (0.1V, 0xFF when not populated)
//   30..31  CRC16, least-significant byte first
constexpr uint8_t V5_VERSION_OFFSET = 3;
constexpr uint8_t V5_FRAME_TYPE_OFFSET = 4;
constexpr uint8_t V5_THROTTLE_OFFSET = 9;
constexpr uint8_t V5_RPM_OFFSET = 13;
constexpr uint8_t V5_VOLTAGE_OFFSET = 15;
constexpr uint8_t V5_CURRENT_OFFSET = 17;
constexpr uint8_t V5_FET_TEMP_OFFSET = 19;
constexpr uint8_t V5_BEC_VOLTAGE_OFFSET = 22;

constexpr uint8_t V5_PROTOCOL_VERSION = 3;
constexpr uint8_t V5_FRAME_TYPE_LOW = 0x30;
constexpr uint8_t V5_FRAME_TYPE_HIGH = 0x5C;
constexpr uint8_t V5_FIELD_ABSENT = 0xFF;

constexpr uint8_t V5_RPM_SCALE = 10;
constexpr uint8_t V5_TEMPERATURE_SCALE = 10;
constexpr uint16_t V5_BEC_VOLTAGE_SCALE = 100;
constexpr uint16_t V5_CRC_POLYNOMIAL = 0xA001; // reflected 0x8005 (CRC-16/MODBUS)

constexpr uint8_t SLOW_TELEMETRY_INTERVAL = 5;

uint32_t toBigEndian24(uint32_t value)
{
#if (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
    return value;
#else
    return __builtin_bswap32(value) >> 8;
#endif
}

}

void SerialHobbywing_TLM::processBytes(uint8_t *bytes, uint16_t size)
{
    const uint32_t now = millis();
    if (framePosition != 0 && partialFrameTimedOut(now, lastReceivedByteMs))
    {
        resetParser();
    }

    if (size != 0)
    {
        lastReceivedByteMs = now;
    }

    for (uint16_t i = 0; i < size; ++i)
    {
        if (protocolV5)
        {
            processByteV5(bytes[i]);
        }
        else
        {
            processByteV4(bytes[i]);
        }
    }

    if (protocolConfirmed)
    {
        return;
    }

    // Until a protocol is confirmed, the other one is tried after a dwell period
    // without a valid frame.
    if (probeStartMs == 0)
    {
        probeStartMs = now;
    }
    else if (static_cast<uint32_t>(now - probeStartMs) >= PROBE_DWELL_MS)
    {
        switchProtocol(now);
    }
}

void SerialHobbywing_TLM::processByteV4(uint8_t value)
{
    // Hunting for the sync byte also skips the 0xB9 trailing marker between frames.
    if (framePosition == 0 && value != V4_SYNC_BYTE)
    {
        return;
    }

    frame[framePosition++] = value;

    if (framePosition == V4_INFO_FRAME_LENGTH && isInfoFrameV4())
    {
        applyInfoFrameV4();
        confirmProtocol(V4_INFO_CONFIRM_FRAMES);
        resetParser();
        return;
    }

    if (framePosition != V4_DATA_FRAME_LENGTH)
    {
        return;
    }

    if (validateDataFrameV4() && decodeDataFrameV4())
    {
        if (confirmProtocol(V4_DATA_CONFIRM_FRAMES))
        {
            scheduleTelemetry();
        }
        resetParser();
    }
    else
    {
        probeFrames = 0;
        resynchronizeParser(V4_SYNC_BYTE);
    }
}

void SerialHobbywing_TLM::processByteV5(uint8_t value)
{
    if (framePosition == 0 && value != V5_SYNC_BYTE)
    {
        return;
    }

    frame[framePosition++] = value;

    if (framePosition != V5_FRAME_LENGTH)
    {
        return;
    }

    if (validateFrameV5())
    {
        decodeFrameV5();
        if (confirmProtocol(V5_CONFIRM_FRAMES))
        {
            scheduleTelemetry();
        }
        resetParser();
    }
    else
    {
        probeFrames = 0;
        resynchronizeParser(V5_SYNC_BYTE);
    }
}

void SerialHobbywing_TLM::resetParser()
{
    framePosition = 0;
}

void SerialHobbywing_TLM::resynchronizeParser(uint8_t syncByte)
{
    // Retain the longest suffix that may be the start of the next frame.
    uint8_t syncPosition = 1;
    while (syncPosition < framePosition && frame[syncPosition] != syncByte)
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
}

void SerialHobbywing_TLM::switchProtocol(uint32_t now)
{
    protocolV5 = !protocolV5;
    port.updateBaudRate(protocolV5 ? V5_BAUD : V4_BAUD);

    // Discard bytes received at the previous baud rate.
    while (port.available() > 0)
    {
        port.read();
    }

    resetParser();
    probeFrames = 0;
    probeStartMs = now;
}

bool SerialHobbywing_TLM::confirmProtocol(uint8_t requiredFrames)
{
    if (!protocolConfirmed)
    {
        // A valid frame restarts the dwell. The baud rate is kept once enough
        // consecutive valid frames have been received.
        probeStartMs = lastReceivedByteMs;
        if (++probeFrames >= requiredFrames)
        {
            protocolConfirmed = true;
            DBGLN("Hobbywing %s telemetry detected", protocolV5 ? "V5" : "V4");
        }
    }

    return protocolConfirmed;
}

bool SerialHobbywing_TLM::isInfoFrameV4() const
{
    // A second sync byte, RPM steps of 1 and the trailing marker identify an INFO frame.
    // The scales are checked when applied, so a zero scale does not drop the frame.
    return frame[1] == V4_SYNC_BYTE && frame[V4_INFO_RPM_STEPS_OFFSET] == 1 &&
        frame[V4_INFO_FRAME_LENGTH - 1] == V4_TRAILING_MARKER;
}

bool SerialHobbywing_TLM::validateDataFrameV4() const
{
    if (frame[V4_THROTTLE_OFFSET] >= V4_THROTTLE_HIGH_BYTE_LIMIT ||
        frame[V4_PWM_OFFSET] >= V4_THROTTLE_HIGH_BYTE_LIMIT)
    {
        return false;
    }

    if (frame[V4_VOLTAGE_ADC_OFFSET] >= V4_ADC_HIGH_BYTE_LIMIT ||
        frame[V4_CURRENT_ADC_OFFSET] >= V4_ADC_HIGH_BYTE_LIMIT ||
        frame[V4_FET_TEMP_ADC_OFFSET] >= V4_ADC_HIGH_BYTE_LIMIT ||
        frame[V4_CAP_TEMP_ADC_OFFSET] >= V4_ADC_HIGH_BYTE_LIMIT)
    {
        return false;
    }

    return frame[V4_RPM_OFFSET] < V4_RPM_HIGH_BYTE_LIMIT;
}

bool SerialHobbywing_TLM::validateFrameV5() const
{
    if (frame[V5_VERSION_OFFSET] != V5_PROTOCOL_VERSION ||
        frame[V5_FRAME_TYPE_OFFSET] != V5_FRAME_TYPE_LOW || frame[V5_FRAME_TYPE_OFFSET + 1] != V5_FRAME_TYPE_HIGH)
    {
        return false;
    }

    return calculateCrc(frame, V5_CRC_OFFSET) == readU16LE(frame + V5_CRC_OFFSET);
}

void SerialHobbywing_TLM::applyInfoFrameV4()
{
    // Each scale is applied only when non-zero so an INFO frame with a zero
    // scale keeps the previously decoded one.
    const uint8_t voltageNumerator = frame[V4_INFO_VOLTAGE_OFFSET];
    const uint8_t voltageDenominator = frame[V4_INFO_VOLTAGE_OFFSET + 1];
    if (voltageNumerator != 0 && voltageDenominator != 0)
    {
        calibration.voltageNumerator = voltageNumerator;
        calibration.voltageDenominator = voltageDenominator;
    }

    const uint8_t currentNumerator = frame[V4_INFO_CURRENT_OFFSET];
    const uint8_t currentDenominator = frame[V4_INFO_CURRENT_OFFSET + 1];
    if (currentNumerator != 0 && currentDenominator != 0)
    {
        calibration.currentNumerator = currentNumerator;
        calibration.currentDenominator = currentDenominator;
        calibration.currentOffset = frame[V4_INFO_CURRENT_OFFSET + 2];
    }
}

bool SerialHobbywing_TLM::decodeDataFrameV4()
{
    const uint16_t rawThrottle = readU16BE(frame + V4_THROTTLE_OFFSET);
    const uint32_t voltageAdc = readU16BE(frame + V4_VOLTAGE_ADC_OFFSET);
    const uint32_t currentAdc = readU16BE(frame + V4_CURRENT_ADC_OFFSET);
    const uint16_t fetTempAdc = readU16BE(frame + V4_FET_TEMP_ADC_OFFSET);

    decoded = {};
    decoded.rpm = readU24BE(frame + V4_RPM_OFFSET);

    // A railed NTC reading must not veto battery and RPM, so an out-of-range
    // conversion only skips the temperature sensor.
    decoded.hasTemperature = decodeTemperatureDeciCelsius(fetTempAdc, decoded.temperatureDeciCelsius);

    // The voltage scale is unknown until an INFO frame has been received.
    if (calibration.voltageDenominator == 0)
    {
        return true;
    }

    // Integer conversions rounded to the nearest 0.1 unit. The INFO current offset
    // is in ampere. Without a current scale the voltage is still reported.
    const uint32_t voltageDeciVolts =
        (voltageAdc * calibration.voltageNumerator + calibration.voltageDenominator / 2U) /
        calibration.voltageDenominator;

    uint32_t currentDeciAmps = 0;
    if (calibration.currentDenominator != 0 && rawThrottle != 0)
    {
        const uint32_t rawDeciAmps =
            (currentAdc * calibration.currentNumerator * V4_DECI_AMPS_PER_AMP + calibration.currentDenominator / 2U) /
            calibration.currentDenominator;
        const uint32_t offsetDeciAmps = calibration.currentOffset * V4_DECI_AMPS_PER_AMP;
        currentDeciAmps = rawDeciAmps > offsetDeciAmps ? rawDeciAmps - offsetDeciAmps : 0U;
    }

    if (voltageDeciVolts > V4_MAX_VOLTAGE_DECI_VOLTS || currentDeciAmps > V4_MAX_CURRENT_DECI_AMPS)
    {
        return false;
    }

    decoded.voltageDeciVolts = static_cast<uint16_t>(voltageDeciVolts);
    decoded.currentDeciAmps = static_cast<uint16_t>(currentDeciAmps);
    decoded.hasBattery = true;
    return true;
}

void SerialHobbywing_TLM::decodeFrameV5()
{
    decoded = {};
    decoded.rpm = static_cast<uint32_t>(readU16LE(frame + V5_RPM_OFFSET)) * V5_RPM_SCALE;
    decoded.voltageDeciVolts = readU16LE(frame + V5_VOLTAGE_OFFSET);
    decoded.temperatureDeciCelsius = static_cast<int16_t>(frame[V5_FET_TEMP_OFFSET]) * V5_TEMPERATURE_SCALE;
    decoded.hasBattery = true;
    decoded.hasTemperature = true;

    // The ESC repeats the last nonzero current while the motor spools down.
    if (frame[V5_THROTTLE_OFFSET] != 0)
    {
        decoded.currentDeciAmps = readU16LE(frame + V5_CURRENT_OFFSET);
    }

    if (frame[V5_BEC_VOLTAGE_OFFSET] != V5_FIELD_ABSENT)
    {
        decoded.becMillivolts = static_cast<uint16_t>(frame[V5_BEC_VOLTAGE_OFFSET]) * V5_BEC_VOLTAGE_SCALE;
        decoded.hasBecVoltage = true;
    }
}

void SerialHobbywing_TLM::scheduleTelemetry()
{
    ++validFrameCount;

    // Battery and RPM are sent on alternating frames. Temperature and BEC voltage
    // are interleaved at a lower rate to avoid consuming the telemetry link.
    if (decoded.hasBattery && (validFrameCount & 1U) != 0)
    {
        sendCRSFbattery();
    }
    else
    {
        sendCRSFrpm();
    }

    if (validFrameCount % SLOW_TELEMETRY_INTERVAL == 0)
    {
        if (decoded.hasTemperature && (sendTemperatureNext || !decoded.hasBecVoltage))
        {
            sendCRSFtemp();
        }
        else if (decoded.hasBecVoltage)
        {
            sendCRSFbecVoltage();
        }
        sendTemperatureNext = !sendTemperatureNext;
    }
}

void SerialHobbywing_TLM::sendCRSFbattery()
{
    crsfBatterySensorDetected = true;

    // Pack voltage and current use the standard CRSF battery sensor. Neither protocol
    // reports consumed capacity, so that field stays zero.
    CRSF_MK_FRAME_T(crsf_sensor_battery_t) crsfBattery {};
    crsfBattery.p.voltage = htobe16(decoded.voltageDeciVolts);
    crsfBattery.p.current = htobe16(decoded.currentDeciAmps);

    crsfRouter.SetHeaderAndCrc(&crsfBattery.h, CRSF_FRAMETYPE_BATTERY_SENSOR,
        CRSF_FRAME_SIZE(sizeof(crsf_sensor_battery_t)));
    crsfRouter.deliverMessageTo(CRSF_ADDRESS_RADIO_TRANSMITTER, &crsfBattery.h);
}

void SerialHobbywing_TLM::sendCRSFrpm()
{
    CRSF_MK_FRAME_T(crsf_sensor_rpm_t) crsfRpm {};
    crsfRpm.p.source_id = RPM_SOURCE_ID;
    crsfRpm.p.rpm0 = toBigEndian24(decoded.rpm);

    constexpr uint8_t payloadSize = SIZE_8BIT + SIZE_24BIT;
    crsfRouter.SetHeaderAndCrc(&crsfRpm.h, CRSF_FRAMETYPE_RPM, CRSF_FRAME_SIZE(payloadSize));
    crsfRouter.deliverMessageTo(CRSF_ADDRESS_RADIO_TRANSMITTER, &crsfRpm.h);
}

void SerialHobbywing_TLM::sendCRSFtemp()
{
    CRSF_MK_FRAME_T(crsf_sensor_temp_t) crsfTemperature {};
    crsfTemperature.p.source_id = TEMPERATURE_SOURCE_ID;
    crsfTemperature.p.temperature[0] = htobe16(static_cast<uint16_t>(decoded.temperatureDeciCelsius));

    constexpr uint8_t payloadSize = SIZE_8BIT + SIZE_16BIT;
    crsfRouter.SetHeaderAndCrc(&crsfTemperature.h, CRSF_FRAMETYPE_TEMP, CRSF_FRAME_SIZE(payloadSize));
    crsfRouter.deliverMessageTo(CRSF_ADDRESS_RADIO_TRANSMITTER, &crsfTemperature.h);
}

void SerialHobbywing_TLM::sendCRSFbecVoltage()
{
    // Source IDs 128 and above are shown as independent Volt sensors by EdgeTX.
    CRSF_MK_FRAME_T(crsf_sensor_cells_t) crsfBecVoltage {};
    crsfBecVoltage.p.source_id = BEC_VOLTAGE_SOURCE_ID;
    crsfBecVoltage.p.cell[0] = htobe16(decoded.becMillivolts);

    constexpr uint8_t payloadSize = SIZE_8BIT + SIZE_16BIT;
    crsfRouter.SetHeaderAndCrc(&crsfBecVoltage.h, CRSF_FRAMETYPE_CELLS, CRSF_FRAME_SIZE(payloadSize));
    crsfRouter.deliverMessageTo(CRSF_ADDRESS_RADIO_TRANSMITTER, &crsfBecVoltage.h);
}

uint16_t SerialHobbywing_TLM::readU16BE(const uint8_t *data)
{
    return static_cast<uint16_t>(data[0]) << 8U |
        static_cast<uint16_t>(data[1]);
}

uint32_t SerialHobbywing_TLM::readU24BE(const uint8_t *data)
{
    return static_cast<uint32_t>(data[0]) << 16U |
        static_cast<uint32_t>(data[1]) << 8U |
        static_cast<uint32_t>(data[2]);
}

uint16_t SerialHobbywing_TLM::readU16LE(const uint8_t *data)
{
    return static_cast<uint16_t>(data[0]) |
        static_cast<uint16_t>(data[1]) << 8U;
}

uint16_t SerialHobbywing_TLM::calculateCrc(const uint8_t *data, size_t length)
{
    // Crc2Byte is MSB-first, while Hobbywing V5 uses the LSB-first reflected CRC-16/MODBUS.
    // Keeping this local avoids input/output bit reversal and shared helper changes.
    uint16_t crc = 0xFFFF;

    while (length-- != 0)
    {
        crc ^= *data++;

        for (uint8_t bit = 0; bit < 8; ++bit)
        {
            crc = (crc & 1U) != 0
                ? static_cast<uint16_t>((crc >> 1U) ^ V5_CRC_POLYNOMIAL)
                : static_cast<uint16_t>(crc >> 1U);
        }
    }

    return crc;
}

bool SerialHobbywing_TLM::partialFrameTimedOut(uint32_t now, uint32_t lastReceived)
{
    return static_cast<uint32_t>(now - lastReceived) >= PARTIAL_FRAME_TIMEOUT_MS;
}

bool SerialHobbywing_TLM::decodeTemperatureDeciCelsius(uint16_t adc, int16_t &outDeciCelsius)
{
    // adc is below 4096 (enforced by validateDataFrameV4), so index + 1 stays in the table.
    const uint16_t index = adc / TEMPERATURE_TABLE_STEP;
    const uint16_t remainder = adc % TEMPERATURE_TABLE_STEP;
    const int32_t base = static_cast<int16_t>(pgm_read_word(&TEMPERATURE_DECI_CELSIUS_TABLE[index]));
    const int32_t next = static_cast<int16_t>(pgm_read_word(&TEMPERATURE_DECI_CELSIUS_TABLE[index + 1]));
    const int32_t deciCelsius = base + (next - base) * remainder / TEMPERATURE_TABLE_STEP;

    if (deciCelsius < MIN_TEMPERATURE_DECI_CELSIUS || deciCelsius > MAX_TEMPERATURE_DECI_CELSIUS)
    {
        return false;
    }

    outDeciCelsius = static_cast<int16_t>(deciCelsius);
    return true;
}

#endif

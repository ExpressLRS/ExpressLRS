#include <math.h>
#include <Arduino.h>
#include <Wire.h>

#include "baro_base.h"
#include "helpers.h"

uint8_t BaroI2CBase::m_address = 0;

/**
 * @brief: Return altitude in cm from pressure in deci-Pascals
 **/
int32_t BaroBase::pressureToAltitude(uint32_t pressuredPa)
{
#if defined(PLATFORM_ESP32)
    const float seaLeveldPa = 1013250; // 1013.25hPa
    return 4433000 * (1.0f - powf(pressuredPa / seaLeveldPa, 0.1903f));
#else // ESP8266
    // Standard atmosphere pressure at 0, 1000, 2000 ... 5000m, linearly interpolated
    // between points. Outside the table the first/last segment is extrapolated,
    // with the error growing exponentially past 5000m
    static const int32_t pressureTable[] = { 1013250, 898750, 794950, 701080, 616400, 540200 };
    const int32_t ALT_STEP_CM = 100000;
    const unsigned LAST_SEG = ARRAY_SIZE(pressureTable) - 2;

    unsigned i = 0;
    while (i < LAST_SEG && (int32_t)pressuredPa < pressureTable[i + 1])
        i++;

    // 64-bit multiply: delta (up to ~115000dPa per segment) * ALT_STEP_CM overflows int32
    const int32_t p0 = pressureTable[i];
    const int32_t p1 = pressureTable[i + 1];
    return i * ALT_STEP_CM + (int64_t)(p0 - (int32_t)pressuredPa) * ALT_STEP_CM / (p0 - p1);
#endif
}

void BaroI2CBase::readRegister(uint8_t reg, uint8_t *data, size_t size)
{
    Wire.beginTransmission(m_address);
    Wire.write(reg);
    if (Wire.endTransmission() == 0)
    {
        Wire.requestFrom(m_address, size);
        Wire.readBytes(data, size);
    }
}

void BaroI2CBase::writeRegister(uint8_t reg, uint8_t *data, size_t size)
{
    Wire.beginTransmission(m_address);
    Wire.write(reg);
    Wire.write(data, size);
    Wire.endTransmission();
}

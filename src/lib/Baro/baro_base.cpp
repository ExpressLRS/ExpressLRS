#include <math.h>
#include <Arduino.h>
#include <Wire.h>

#include "baro_base.h"

uint8_t BaroI2CBase::m_address = 0;

/**
 * @brief: Return altitude in cm from pressure in deci-Pascals
 **/
int32_t BaroBase::pressureToAltitude(uint32_t pressuredPa)
{
#if defined(PLATFORM_ESP32)
     const float seaLeveldPa = 1013250; // 1013.25hPa
     return 4433000 * (1.0 - pow(pressuredPa / seaLeveldPa, 0.1903));
#else // ESP8266
    const size_t LUT_CNT = 6;
    const int32_t pressureTable[LUT_CNT] = { 1013250, 898750, 794950, 701080, 616400, 540200 };
    const int32_t altitudeTable[LUT_CNT] = { 0, 100000, 200000, 300000, 400000, 500000 };

    unsigned i = 0;
    while (i < LUT_CNT-2 && (int32_t)pressuredPa < pressureTable[i + 1])
        i++;

    // Linear interpolation, note that >5000m the error grows exponentially
    int32_t p0 = pressureTable[i];
    int32_t p1 = pressureTable[i + 1];
    int32_t a0 = altitudeTable[i];
    int32_t a1 = altitudeTable[i + 1];

    return map(pressuredPa, p0, p1, a0, a1);
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

#include <math.h>
#include <Wire.h>

#include "baro_base.h"
#include <Arduino.h>

uint8_t BaroI2CBase::m_address = 0;

/**
 * @brief: Return altitude in cm from pressure in deci-Pascals
 **/
// int32_t BaroBase::pressureToAltitude(uint32_t pressuredPa)
// {
//     const float seaLeveldPa = 1013250; // 1013.25hPa
//     return 4433000 * (1.0 - pow(pressuredPa / seaLeveldPa, 0.1903));
// }

int32_t BaroBase::pressureToAltitude(uint32_t pressurePa)
{
    const size_t LUT_CNT = 6;
    const uint32_t pressureTable[LUT_CNT] = { 101325, 89875, 79495, 70108, 61640, 54020 };
    const uint32_t altitudeTable[LUT_CNT] = { 0, 100000, 200000, 300000, 400000, 500000 };

    if (pressurePa >= pressureTable[0])
        return 0;
    if (pressurePa <= pressureTable[LUT_CNT-1])
        return altitudeTable[LUT_CNT-1];

    int8_t i = 0;
    while (pressurePa < pressureTable[i + 1])
        i++;

    // Linear interpolation
    uint32_t p0 = pressureTable[i];
    uint32_t p1 = pressureTable[i + 1];
    uint32_t a0 = altitudeTable[i];
    uint32_t a1 = altitudeTable[i + 1];

    //return a0 + ((a1 - a0) * (p0 - (int32_t)pressurePa)) / (p0 - p1);
    return map(pressurePa, p0, p1, a0, a1);
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

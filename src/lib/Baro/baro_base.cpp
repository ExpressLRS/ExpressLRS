#include <math.h>
#include <Wire.h>

#include "baro_base.h"

uint8_t BaroI2CBase::m_address = 0;

/**
 * @brief: Return altitude in cm from pressure in deci-Pascals
 **/
int32_t BaroBase::pressureToAltitude(uint32_t pressuredPa)
{
    const float seaLeveldPa = 1013250; // 1013.25hPa
    return 4433000 * (1.0 - pow(pressuredPa / seaLeveldPa, 0.1903));
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

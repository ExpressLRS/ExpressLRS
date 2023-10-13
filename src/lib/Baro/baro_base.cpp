#include <math.h>
#include <Wire.h>

#include "baro_base.h"

/**
 * @brief: Return altitude in cm from pressure in deci-Pascals
 **/
int32_t BaroBase::pressureToAltitude(uint32_t pressuredPa)
{
    const float seaLeveldPa = 1013250; // 1013.25hPa
    return 4433000 * (1.0 - pow(pressuredPa / seaLeveldPa, 0.1903));
}

void BaroI2CBase::writeRegister(uint8_t reg, uint8_t *data, size_t size)
{
    const uint16_t address = getI2CAddress();
    Wire.beginTransmission(address);
    Wire.write(reg);
    Wire.write(data, size);
    Wire.endTransmission();
}

void BaroI2CBase::readRegister(uint8_t reg, uint8_t *data, size_t size)
{
    const uint16_t address = getI2CAddress();
    Wire.beginTransmission(address);
    Wire.write(reg);
    if (Wire.endTransmission() == 0)
    {
        Wire.requestFrom(address, size);
        Wire.readBytes(data, size);
    }
}


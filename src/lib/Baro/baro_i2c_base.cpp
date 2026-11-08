#include <Arduino.h>
#include <Wire.h>

#include "baro_base.h"

uint8_t BaroI2CBase::m_address = 0;

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

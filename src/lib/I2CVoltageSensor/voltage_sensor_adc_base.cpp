#include "voltage_sensor_adc_base.h"
#include <Wire.h>
#include <stdint.h>

size_t I2CVoltageSensorADCBase::readRegister(uint8_t reg, uint8_t *data, size_t size)
{
    return readRegister(m_address, reg, data, size);
}

size_t I2CVoltageSensorADCBase::readRegister(uint8_t address, uint8_t reg, uint8_t *data, size_t size)
{

    Wire.beginTransmission(address);
    Wire.write(reg);
    if (Wire.endTransmission() == 0)
    {
        Wire.requestFrom(address, size);
        return Wire.readBytes(data, size);
    }
    return 0;
}

uint8_t I2CVoltageSensorADCBase::writeRegister(uint8_t address, uint8_t reg, const uint8_t *data, size_t size)
{
    Wire.beginTransmission(address);
    Wire.write(reg);
    Wire.write(data, size);
    return Wire.endTransmission();
}

uint8_t I2CVoltageSensorADCBase::writeRegister(uint8_t reg, const uint8_t *data, size_t size)
{
    return writeRegister(m_address, reg, data, size);
}

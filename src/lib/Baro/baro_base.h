#pragma once

#include <stdint.h>
#include <stddef.h>

class BaroBase
{
public:
    static const int32_t ALTITUDE_INVALID = 0x7fffffff;
    static const int32_t TEMPERATURE_INVALID = 0x7fffffff;
    static const uint32_t PRESSURE_INVALID = 0xffffffff;

    BaroBase() : m_initialized(false), m_altitudeHome(ALTITUDE_INVALID) {}

    virtual void initialize() = 0;
    // Return expected duration of pressure measurement (ms)
    virtual uint8_t getPressureDuration() = 0;
    // Start reading pressure
    virtual void startPressure() = 0;
    // Get pressure result (deci-Pascals)
    virtual uint32_t getPressure() = 0;
    // Return expected duration of temperature measurement (ms)
    virtual uint8_t getTemperatureDuration() = 0;
    // Start reading temperature
    virtual void startTemperature() = 0;
    // Get temperature result (centiDegrees)
    virtual int32_t getTemperature() = 0;

    // Base functions
    bool isInitialized() const { return m_initialized; }
    int32_t pressureToAltitude(uint32_t pressurePa);
    // Properties
    int32_t getAltitudeHome() const { return m_altitudeHome; }
    void setAltitudeHome(int32_t altitudeHome) { m_altitudeHome = altitudeHome; }
protected:
    bool m_initialized;
    int32_t m_altitudeHome;
};

void I2cReadRegister(const uint16_t address, uint8_t reg, uint8_t *data, size_t size);
void I2cWriteRegister(const uint16_t address, uint8_t reg, uint8_t *data, size_t size);

template<uint8_t addr> class BaroI2CBase : public BaroBase
{
protected:
    static void readRegister(uint8_t reg, uint8_t *data, size_t size) { I2cReadRegister(addr, reg, data, size); }
    static void writeRegister(uint8_t reg, uint8_t *data, size_t size) { I2cWriteRegister(addr, reg, data, size); }
};

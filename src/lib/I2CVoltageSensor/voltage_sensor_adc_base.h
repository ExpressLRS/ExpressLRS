#pragma once
#include <optional>
#include <stddef.h>
#include <stdint.h>

typedef /*std::optional<*/ uint32_t /*>*/ I2CVoltageSensorReadingResult;

class I2CVoltageSensorADCBase
{
protected:
    uint8_t m_address;

    static size_t readRegister(uint8_t address, uint8_t reg, uint8_t *data, size_t size);
    static uint8_t writeRegister(uint8_t address, uint8_t reg, const uint8_t *data, size_t size);
    size_t readRegister(uint8_t reg, uint8_t *data, size_t size);
    uint8_t writeRegister(uint8_t reg, const uint8_t *data, size_t size);

public:
    I2CVoltageSensorADCBase(uint8_t address)
        : m_address(address) {}

    // Configure the ADC chip
    virtual void configure();

    // Does the ADC have (new?) data available?
    virtual bool isReady();

    // Do some update work, used to drive internal state machines.
    // Should update internal information such that isReady returns
    // true if there is (new) data available;
    virtual int update();

    // Get the number of ADC channels available
    virtual uint8_t getNumChannels() const;

    // Retrieve a reading for the given channel. Returns a
    virtual I2CVoltageSensorReadingResult getReading(uint8_t channel);
};

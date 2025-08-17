#pragma once

#include "voltage_sensor_adc_base.h"

#include "ads1115_parameters.h"

#include <cstdint>
#include <stdint.h>

class ADS1115 : public I2CVoltageSensorADCBase
{
    enum class ADS1115State
    {
        Undefined,
        Idle,
        WaitingForData,
    };
    ADS1115State m_state = ADS1115State::Undefined;
    uint8_t m_current_channel = 0;
    bool valuesAvailable = false;

    uint16_t m_values[4] = {0};

    ADS1115_config_register_t readConfig();

    uint8_t writeRegister(ADS1115Register reg, uint16_t value);
    size_t readRegister(ADS1115Register reg, uint16_t &value);

public:
    ADS1115(ADS1115Address addr);

    static bool detect(ADS1115Address addr = ADS1115Address::GND);

    void configure();
    /* 	// Base class functions */

    int update();
    bool isReady();

    uint8_t getNumChannels() const;
    I2CVoltageSensorReadingResult getReading(uint8_t channel);
};

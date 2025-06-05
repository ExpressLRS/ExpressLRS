#include "ads1115.h"
#include "common.h"
#include "device.h"
#include "logging.h"
#include "voltage_sensor_adc_base.h"

#include "ads1115_parameters.h"

#include <cstdint>

#define ADS1115_CONFIG_VALUE_LENGTH 2

constexpr uint8_t _reg(ADS1115Register value)
{
    return static_cast<uint8_t>(value);
}
constexpr uint8_t _addr(ADS1115Address value)
{
    return static_cast<uint8_t>(value);
}

uint8_t ADS1115::writeRegister(ADS1115Register reg, uint16_t value)
{
    const uint8_t data[2] = {((uint8_t)(value >> 8)), ((uint8_t)(value & 0xFF))};
    auto r = I2CVoltageSensorADCBase::writeRegister(_reg(reg), &data[0], sizeof(data));
    return r;
}

size_t ADS1115::readRegister(ADS1115Register reg, uint16_t &value)
{
    uint8_t data[2] = {((uint8_t)(-1)), ((uint8_t)(-1))};
    auto r = I2CVoltageSensorADCBase::readRegister(_reg(reg), &data[0], sizeof(data));
    value = data[0] << 8 | data[1];
    return r;
}

ADS1115::ADS1115(ADS1115Address addr)
    : I2CVoltageSensorADCBase(_addr(addr)) {}

bool ADS1115::detect(ADS1115Address addr)
{

    ADS1115_config_register_union_t v;
    // set the device to default values
    uint8_t init[ADS1115_CONFIG_VALUE_LENGTH] = {0x85, 0x83};
    I2CVoltageSensorADCBase::writeRegister(_addr(addr), _reg(ADS1115Register::Config), &init[0], sizeof(init));

    // verify the configuration is in the documented reset state
    uint8_t config_value[ADS1115_CONFIG_VALUE_LENGTH] = {0, 0};
    I2CVoltageSensorADCBase::readRegister(_addr(addr), _reg(ADS1115Register::Config), &config_value[0], sizeof(config_value));
    DBGLN("Response from ADS1115 discovery: %x%x", config_value[0], config_value[1]);
    v.val = config_value[0] << 8 | config_value[1];
    // v.fields.prittyPrint();

    // After reset the register value is expected to be 0x8583 or
    // 0x0583 if the power-on reset triggered conversion is still
    // ongoing
    if ((config_value[0] == 0x85 || config_value[0] == 0x05) && config_value[1] == 0x83)
        return true;

    return false;
}

inline uint16_t config_register_value(const ADS1115MuxInput channel = ADS1115MuxInput::DEFAULT_INPUT, const bool os = false, const ADS1115DataRate rate = ADS1115DataRate::_128SPS, const ADS1115PGAFSR gain = ADS1115PGAFSR::_2_048V, const ADS1115Mode _mode = ADS1115Mode::SINGLE)
{

    ADS1115_config_register_union_t config;
    config.fields.os = os ? 1 : 0;
    config.fields.mux = ADS1115MuxInputToUint8(channel);
    config.fields.pga = ADS1115PGAFSRToUint8(gain);
    config.fields.mode = ADS1115ModeToUint8(_mode);
    config.fields.dr = ADS1115DataRateToUint8(rate);
    config.fields.comp_mode = 0b0;
    config.fields.comp_pol = 0b0;
    config.fields.comp_lat = 0b0;
    config.fields.comp_que = 0b11;

#if 0
    uint16_t configuration = 0 |
                             ((os ? 1 : 0) & 0b1) << 15 |
                             ((ADS1115MuxInputToUint8(channel) & 0b111) << 12) |
                             ((ADS1115PGAFSRToUint8(gain) & 0b111) << 9) |
                             ((ADS1115ModeToUint8(_mode) & 0b1) << 8) |

                             ((ADS1115DataRateToUint8(rate) & 0b111) << 5) |
                             ((0 & 0b1) << 4) |
                             ((0 & 0b1) << 3) |
                             ((0 & 0b1) << 2) |
                             ((0b11 & 0b11) << 0);

    if (config.val != configuration)
	{
        DBGLN("configuration value: %x%x =?= %x", (config.val >> 8), config.val & 0xff, configuration);
		config.fields.prettyPrint();
	}
#endif

    return config.val;
}

void ADS1115::configure()
{
    // DBGLN("Configuring");
    const auto value = config_register_value();
    writeRegister(ADS1115Register::Config, value);

    // DBGLN("Writing thresholds");
    //  after changing PSR we need to also update the threshold registers
    const uint16_t lo = 0x8000;
    writeRegister(ADS1115Register::Lo_thresh, lo);
    const uint16_t hi = 0x7FFF;
    writeRegister(ADS1115Register::Hi_thresh, hi);
}

ADS1115_config_register_t ADS1115::readConfig()
{
    ADS1115_config_register_union_t u;
    readRegister(ADS1115Register::Config, u.val);
    return u.fields;
}

int ADS1115::update()
{
    const auto dr = ADS1115DataRate::_128SPS;
    auto update_time = ADS1115DataRateToUpdateTimeMS(dr);

    // DBGLN("update");

    switch (m_state)
    {
    case ADS1115State::Undefined: {
        // check if the device is idle (highest bit in configuration register == 1)
        const auto config = readConfig();
        if (config.os)
        {
            m_state = ADS1115State::Idle;
        }
        else
        {
            // Wait for device to become idle.
            DBGLN("Waiting for device to become idle");
            return ADS1115DataRateToUpdateTimeMS(ADS1115DataRateFromUint8(config.dr));
        }
    }
        // fallthrough
    case ADS1115State::Idle: {
        m_current_channel = 0;

        // DBGLN("Device is idle");
        const uint16_t config = config_register_value(ADS1115MuxInput::AIN0_GND, true, dr);
        writeRegister(ADS1115Register::Config, config);
        m_state = ADS1115State::WaitingForData;

        break;
    }
    case ADS1115State::WaitingForData: {
        const auto config = readConfig();
        // DBGLN("Waiting for data");
        if (config.os)
        {
            // measurment is done
            uint16_t result;
            readRegister(ADS1115Register::Conversion, result);
            // DBGLN("result: %x %x", m_current_channel, result);
            const uint16_t measurment = result;
            m_values[m_current_channel] = measurment;
            m_current_channel += 1;

            if (m_current_channel >= 4)
            {
                // once we've collected all the values we mark data as ready
                valuesAvailable = true;
                m_state = ADS1115State::Idle;
                update_time = 1000; // FIXME
                break;
            }
            auto next_channel = m_current_channel == 0 ? ADS1115MuxInput::AIN0_GND : (m_current_channel == 1 ? ADS1115MuxInput::AIN1_GND : (m_current_channel == 2 ? ADS1115MuxInput::AIN2_GND : (m_current_channel == 3 ? ADS1115MuxInput::AIN3_GND : /* unreachable */ ADS1115MuxInput::AIN0_GND)));

            const uint16_t config = config_register_value(next_channel, true, dr);
            writeRegister(ADS1115Register::Config, config);

            m_state = ADS1115State::WaitingForData;
            return update_time;
        }
        break;
    }
    }

    return update_time;

    //    const auto channels = {
    //        ADS1115MuxInput::AIN0_GND,
    //        ADS1115MuxInput::AIN1_GND,
    //        ADS1115MuxInput::AIN2_GND,
    //        ADS1115MuxInput::AIN3_GND};
    //    int i = 0;
}

bool ADS1115::isReady()
{
    // FIXME: once we have a logic that tracks which channels we've updated implement this
    return valuesAvailable;
}

uint8_t ADS1115::getNumChannels() const
{
    return 4;
}

I2CVoltageSensorReadingResult ADS1115::getReading(uint8_t channel)
{
    if (channel >= 4)
        return -1;
    return m_values[channel];
}


#include "DAC.h"

#if DAC_IN_USE && defined(DAC_I2C_ADDRESS)
#include "helpers.h"
#include "logging.h"
#include <Wire.h>

typedef struct {
    uint16_t mW;
    uint8_t dB;
    uint8_t gain;
    uint16_t volts; // APC2volts*1000
} dac_lut_s;

dac_lut_s LUT[] =
{
#if defined(TARGET_R9M_TX)
#if defined(Regulatory_Domain_EU_868)
    {10, 10, 8, 650},
    {25, 14, 12, 860},
    {50, 17, 15, 1000},
    {100, 20, 18, 1160},
    {250, 24, 22, 1420},
    {500, 27, 25, 1730},
    {1000, 30, 28, 2100},
    {2000, 33, 31, 2600}, // Danger untested at high power
#else
    {10, 10, 8, 720},
    {25, 14, 12, 875},
    {50, 17, 15, 1000},
    {100, 20, 18, 1140},
    {250, 24, 22, 1390},
    {500, 27, 25, 1730},
    {1000, 30, 28, 2100},
    {2000, 33, 31, 2600}, // Danger untested at high power
#endif

#elif defined(TARGET_NAMIMNORC_TX)
#if defined(Regulatory_Domain_EU_868)
    {10, 10, 8, 500},
    {25, 14, 12, 860},
    {50, 17, 15, 1000},
    {100, 20, 18, 1170},
    {250, 24, 22, 1460},
    {500, 27, 25, 1730},
    {1000, 30, 28, 2100},
    {2000, 33, 31, 2600},
#else
    {10, 10, 8, 895},
    {25, 14, 12, 1030},
    {50, 17, 15, 1128},
    {100, 20, 18, 1240},
    {250, 24, 22, 1465},
    {500, 27, 25, 1700},
    {1000, 30, 28, 2050},
    {2000, 33, 31, 2600},
#endif

#elif defined(TARGET_TX_ES915TX)
#if defined(Regulatory_Domain_EU_868)
    {10, 10, 8, 375}, // 25mW is the minimum even the value is very low
    {25, 14, 12, 850},
    {50, 17, 15, 1200},
    {100, 20, 18, 1400},
    {250, 24, 22, 1700},
    {500, 27, 25, 2000},
    {1000, 30, 28, 2400}, // not tested, use same as 915
    {2000, 33, 31, 2600}, // not tested, use same as 915
#else
    {10, 10, 8, 875},
    {25, 14, 12, 1065},
    {50, 17, 15, 1200},
    {100, 20, 18, 1355},
    {250, 24, 22, 1600},
    {500, 27, 25, 1900},
    {1000, 30, 28, 2400},
    {2000, 33, 31, 2600}, // Danger untested at high power
#endif
#endif
};

#ifndef DAC_REF_VCC
#define DAC_REF_VCC 3300
#endif

void DAC::init()
{
    DBGLN("Init DAC Driver");

    Wire.setSDA(GPIO_PIN_SDA); // set is needed or it wont work :/
    Wire.setSCL(GPIO_PIN_SCL);
    Wire.begin();
    m_state = UNKNOWN;
}

void DAC::standby()
{
    if (m_state != STANDBY)
    {
        Wire.beginTransmission(DAC_I2C_ADDRESS);
        Wire.write(0x00);
        Wire.write(0x00);
        Wire.endTransmission();
        m_state = STANDBY;
    }
}

void DAC::resume()
{
    if (m_state != RUNNING)
    {
        DAC::setVoltageRegDirect(m_currVoltageRegVal);
    }
}

void DAC::setVoltageMV(uint32_t voltsMV)
{
    uint8_t ScaledVolts = map(voltsMV, 0, DAC_REF_VCC, 0, 255);
    setVoltageRegDirect(ScaledVolts);
    m_currVoltageMV = voltsMV;
    DBGLN("DAC Voltage %dmV", m_currVoltageMV);
}

void DAC::setVoltageRegDirect(uint8_t voltReg)
{
    m_currVoltageRegVal = voltReg;
    uint8_t RegH = ((voltReg & 0b11110000) >> 4) + (0b0000 << 4);
    uint8_t RegL = (voltReg & 0b00001111) << 4;

    Wire.beginTransmission(DAC_I2C_ADDRESS);
    Wire.write(RegH);
    Wire.write(RegL);
    Wire.endTransmission();
}

void DAC::setPower(DAC_PWR_ power)
{
    if (ARRAY_SIZE(LUT) <= power)
        power = (DAC_PWR_)(ARRAY_SIZE(LUT) - 1);
    DAC::setVoltageMV(LUT[power].volts);
}

DAC TxDAC;

#endif // DAC_IN_USE && defined(DAC_I2C_ADDRESS)

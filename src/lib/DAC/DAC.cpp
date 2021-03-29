
#include "DAC.h"

#if DAC_IN_USE

#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))

#ifndef DAC_I2C_ADDRESS
#define DAC_I2C_ADDRESS 0b0001100
#endif /* DAC_I2C_ADDRESS */
#define VCC         3300

typedef struct {
    uint16_t mW;
    uint8_t dB;
    uint8_t gain;
    uint16_t volts; // APC2volts*1000
} dac_lut_s;

#if defined(TARGET_R9M_TX)
#if defined(Regulatory_Domain_EU_868)
dac_lut_s LUT[] = {
    // mw, dB, gain, APC2volts*1000, figures assume 2dBm input
    {10, 10, 8, 650},
    {25, 14, 12, 860},
    {50, 17, 15, 1000},
    {100, 20, 18, 1160},
    {250, 24, 22, 1420},
    {500, 27, 25, 1730},
    {1000, 30, 28, 2100},
    {2000, 33, 31, 2600}, // Danger untested at high power
};
#else
dac_lut_s LUT[] = {
    // mw, dB, gain, APC2volts*1000, figures assume 2dBm input
    {10, 10, 8, 720},
    {25, 14, 12, 875},
    {50, 17, 15, 1000},
    {100, 20, 18, 1140},
    {250, 24, 22, 1390},
    {500, 27, 25, 1730},
    {1000, 30, 28, 2100},
    {2000, 33, 31, 2600}, // Danger untested at high power
};
#endif
#elif defined(TARGET_NAMIMNORC_TX)
#if defined(Regulatory_Domain_EU_868)
dac_lut_s LUT[] = {
    // mw, dB, gain, APC2volts*1000, figures assume 2dBm input
    {10, 10, 8, 315},
    {25, 14, 12, 460},
    {50, 17, 15, 595},
    {100, 20, 18, 750},
    {250, 24, 22, 1125},
    {500, 27, 25, 1505},
    {1000, 30, 28, 2105},
};
#else
dac_lut_s LUT[] = {
    // mw, dB, gain, APC2volts*1000, figures assume 2dBm input
    {10, 10, 8, 460},
    {25, 14, 12, 622},
    {50, 17, 15, 765},
    {100, 20, 18, 1025},
    {250, 24, 22, 1375},
    {500, 27, 25, 1750},
    {1000, 30, 28, 2250},
};
#endif
#elif defined(TARGET_TX_ES915TX)
dac_lut_s LUT[] = {
    // mw, dB, gain, APC2volts*1000, figures assume 2dBm input
    {10, 10, 8, 875},
    {25, 14, 12, 1065},
    {50, 17, 15, 1200},
    {100, 20, 18, 1355},
    {250, 24, 22, 1600},
    {500, 27, 25, 1900},
    {1000, 30, 28, 2400},
    {2000, 33, 31, 2600}, // Danger untested at high power
};
#endif

void DAC::init()
{
    Serial.println("Init DAC Driver");

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
        Radio.SetOutputPower(0b0000);
        DAC::setVoltageRegDirect(m_currVoltageRegVal);
    }
}

void DAC::setVoltageMV(uint32_t voltsMV)
{
    uint8_t ScaledVolts = map(voltsMV, 0, VCC, 0, 255);
    setVoltageRegDirect(ScaledVolts);
    m_currVoltageMV = voltsMV;
    Serial.println(m_currVoltageMV);
}

void DAC::setVoltageRegDirect(uint8_t voltReg)
{
    m_currVoltageRegVal = voltReg;
    uint8_t RegH = ((voltReg & 0b11110000) >> 4) + (0b0000 << 4);
    uint8_t RegL = (voltReg & 0b00001111) << 4;

    Radio.SetOutputPower(0b0000);
    Wire.beginTransmission(DAC_I2C_ADDRESS);
    Wire.write(RegH);
    Wire.write(RegL);
    Wire.endTransmission();
}

void DAC::setPower(DAC_PWR_ power)
{
    Radio.SetOutputPower(0b0000);
    if (ARRAY_SIZE(LUT) <= power)
        power = (DAC_PWR_)(ARRAY_SIZE(LUT) - 1);
    DAC::setVoltageMV(LUT[power].volts);
}

DAC TxDAC;

#endif // DAC_IN_USE

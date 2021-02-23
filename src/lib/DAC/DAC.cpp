
#if defined(TARGET_R9M_TX) || defined(TARGET_TX_ES915TX)

#include "DAC.h"
#include "SX127xDriver.h"

#define DAC_ADDR    0b0001100
#define VCC         3300

#if defined(TARGET_R9M_TX)
int DAC::LUT[8][4] = {
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
#elif defined(TARGET_TX_ES915TX)
int DAC::LUT[8][4] = {
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
    Serial.println("Initialising Wire lib for DAC...");

    Wire.setSDA(GPIO_PIN_SDA); // set is needed or it wont work :/
    Wire.setSCL(GPIO_PIN_SCL);
    Wire.begin();
    m_state = UNKNOWN;
}

void DAC::standby()
{
    if (m_state != STANDBY)
    {
        Wire.beginTransmission(DAC_ADDR);
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
    Wire.beginTransmission(DAC_ADDR);
    Wire.write(RegH);
    Wire.write(RegL);
    Wire.endTransmission();
}

void DAC::setPower(DAC_PWR_ power)
{
    Radio.SetOutputPower(0b0000);
    uint32_t reqVolt = LUT[(uint8_t)power][3];
    DAC::setVoltageMV(reqVolt);
}

#endif
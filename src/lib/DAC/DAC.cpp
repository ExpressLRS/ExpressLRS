
#include "DAC.h"

#if defined(DAC_I2C_ADDRESS)
#include "helpers.h"
#include "logging.h"
#include <Wire.h>

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

void DAC::setPower(int16_t milliVolts)
{
    DAC::setVoltageMV(milliVolts);
}

DAC TxDAC;

#endif // defined(DAC_I2C_ADDRESS)

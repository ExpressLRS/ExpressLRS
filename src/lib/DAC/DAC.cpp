
#include "DAC.h"

#if defined(POWER_OUTPUT_DAC)
#include "helpers.h"
#include "logging.h"
#include <Wire.h>

#ifndef DAC_REF_VCC
#define DAC_REF_VCC 3300
#endif

void DAC::init()
{
    //DBGLN("Init DAC Driver");

    // I2C initialization is the responsibility of the caller
    // e.g. Wire.begin(GPIO_PIN_SDA, GPIO_PIN_SCL);

    m_state = UNKNOWN;
}

void DAC::standby()
{
    if (m_state != STANDBY)
    {
        Wire.beginTransmission(POWER_OUTPUT_DAC);
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

void DAC::setVoltageRegDirect(uint8_t voltReg)
{
    m_currVoltageRegVal = voltReg;
    uint8_t RegH = ((voltReg & 0b11110000) >> 4) + (0b0000 << 4);
    uint8_t RegL = (voltReg & 0b00001111) << 4;

    Wire.beginTransmission(POWER_OUTPUT_DAC2);
    Wire.write(RegH);
    Wire.write(RegL);
    Wire.endTransmission();

    RegH = ((208 & 0b11110000) >> 4) + (0b0000 << 4);
    RegL = (208 & 0b00001111) << 4;

    Wire.beginTransmission(POWER_OUTPUT_DAC);
    Wire.write(RegH);
    Wire.write(RegL);
    Wire.endTransmission();
}
void DAC::setPower(uint32_t milliVolts)
{
    uint8_t ScaledVolts = map(milliVolts, 0, DAC_REF_VCC, 0, 255);
    setVoltageRegDirect(ScaledVolts);
    //DBGLN("DAC::setPower(%umV)", milliVolts);
}


DAC TxDAC;

#endif // defined(POWER_OUTPUT_DAC)

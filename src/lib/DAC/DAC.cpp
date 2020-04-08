
#ifdef TARGET_R9M_TX

#include "DAC.h"
#include "LoRaRadioLib.h"
#include <Arduino.h>
#include <Wire.h>

extern SX127xDriver Radio;

#define VCC 3300

R9DAC::R9DAC()
{
    CurrVoltageMV = 0;
    CurrVoltageRegVal = 0;
    ADDR = 0;

    DAC_State = UNKNOWN;
}

void R9DAC::init(uint8_t sda, uint8_t scl, uint8_t addr)
{
    ADDR = addr;
    DAC_State = UNKNOWN;

    Wire.setSDA(sda); // set is needed or it wont work :/
    Wire.setSCL(scl);
    Wire.begin();
}

void R9DAC::standby()
{
    if (DAC_State != STANDBY)
    {
        Wire.beginTransmission(ADDR);
        Wire.write(0x00);
        Wire.write(0x00);
        Wire.endTransmission();
        DAC_State = STANDBY;
    }
}

void R9DAC::resume()
{
    if (DAC_State != RUNNING)
    {
        Radio.SetOutputPower(0b0000);
        setVoltageRegDirect(CurrVoltageRegVal);
        DAC_State = RUNNING;
    }
}

void R9DAC::setVoltageMV(uint32_t voltsMV)
{
    uint8_t ScaledVolts = map(voltsMV, 0, VCC, 0, 255);
    setVoltageRegDirect(ScaledVolts);
    CurrVoltageMV = voltsMV;
}

void R9DAC::setVoltageRegDirect(uint8_t voltReg)
{
    CurrVoltageRegVal = voltReg;
    uint8_t RegH = ((voltReg & 0b11110000) >> 4) + (0b0000 << 4);
    uint8_t RegL = (voltReg & 0b00001111) << 4;

    Radio.SetOutputPower(0b0000);
    Wire.beginTransmission(ADDR);
    Wire.write(RegH);
    Wire.write(RegL);
    Wire.endTransmission();
}

void R9DAC::setPower(DAC_PWR_ power)
{
    Radio.SetOutputPower(0b0000);
    uint32_t reqVolt = LUT[(uint8_t)power][3];
    setVoltageMV(reqVolt);
}

#endif

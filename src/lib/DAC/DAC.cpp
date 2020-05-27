
#ifdef TARGET_R9M_TX

#include "../../src/targets.h"
#include "DAC.h"
#include "LoRaRadioLib.h"

extern SX127xDriver Radio;

int R9DAC::LUT[8][4] = {
    // mw, dB, gain, APC2volts*1000, figures assume 2dBm input
    {10, 11, 9, 800},
    {25, 14, 12, 920},
    {50, 17, 15, 1030},
    {100, 20, 18, 1150},
    {250, 24, 22, 1330},
    {500, 27, 25, 1520},
    {1000, 30, 28, 1790},
    {2000, 33, 31, 2160},
};

uint32_t R9DAC::CurrVoltageMV = 0;
uint8_t R9DAC::CurrVoltageRegVal = 0;
uint8_t R9DAC::SDA = 0;
uint8_t R9DAC::SCL = 0;
uint8_t R9DAC::ADDR = 0;

DAC_STATE_ R9DAC::DAC_STATE = UNKNOWN;

void R9DAC::init(uint8_t SDA_, uint8_t SCL_, uint8_t ADDR_)
{
    Serial.println("Wire.h begin()");
    Serial.println(SDA_);
    Serial.println(SCL_);
    R9DAC::SDA = SDA_;
    R9DAC::SCL = SCL_;
    R9DAC::ADDR = ADDR_;

    Wire.setSDA(SDA); // set is needed or it wont work :/
    Wire.setSCL(SCL);
    Wire.begin(ADDR);
    R9DAC::DAC_STATE = UNKNOWN;
}

void R9DAC::standby()
{
    if (R9DAC::DAC_STATE != STANDBY)
    {
        Wire.beginTransmission(R9DAC::ADDR);
        Wire.write(0x00);
        Wire.write(0x00);
        Wire.endTransmission();
        R9DAC::DAC_STATE = STANDBY;
    }
}

void R9DAC::resume()
{
    if (R9DAC::DAC_STATE != RUNNING)
    {
        Radio.SetOutputPower(0b0000);
        R9DAC::setVoltageRegDirect(CurrVoltageRegVal);
    }
}

void R9DAC::setVoltageMV(uint32_t voltsMV)
{
    uint8_t ScaledVolts = map(voltsMV, 0, VCC, 0, 255);
    setVoltageRegDirect(ScaledVolts);
    CurrVoltageMV = voltsMV;
    Serial.println(CurrVoltageMV);
}

void R9DAC::setVoltageRegDirect(uint8_t voltReg)
{
    CurrVoltageRegVal = voltReg;
    uint8_t RegH = ((voltReg & 0b11110000) >> 4) + (0b0000 << 4);
    uint8_t RegL = (voltReg & 0b00001111) << 4;

    Radio.SetOutputPower(0b0000);
    Wire.beginTransmission(R9DAC::ADDR);
    Wire.write(RegH);
    Wire.write(RegL);
    Wire.endTransmission();
}

void R9DAC::setPower(DAC_PWR_ power)
{
    Radio.SetOutputPower(0b0000);
    uint32_t reqVolt = LUT[(uint8_t)power][3];
    R9DAC::setVoltageMV(reqVolt);
}

#endif
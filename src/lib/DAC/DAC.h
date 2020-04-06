#pragma once

#ifdef TARGET_R9M_TX

#include "../../src/targets.h"
#include <Wire.h>

typedef enum
{
    R9_PWR_10mW = 0,
    R9_PWR_25mW = 1,
    R9_PWR_50mW = 2,
    R9_PWR_100mW = 3,
    R9_PWR_250mW = 4,
    R9_PWR_500mW = 5,
    R9_PWR_1000mW = 6,
    R9_PWR_2000mW = 7

} DAC_PWR_;

/*typedef enum
{
    UNKNOWN = 0,
    RUNNING = 1,
    STANDBY = 2

} DAC_STATE_;
*/

class R9DAC
{
private:
    const int LUT[8][4] = {
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

    //DAC_STATE_ DAC_State;
    enum
    {
        UNKNOWN = 0,
        RUNNING = 1,
        STANDBY = 2

    } DAC_State;
    uint8_t ADDR;

public:
    uint32_t CurrVoltageMV;
    uint8_t CurrVoltageRegVal;

    R9DAC();
    void init(uint8_t SDA_, uint8_t SCL_, uint8_t ADDR_);
    void standby();
    void resume();
    void setVoltageMV(uint32_t voltsMV);
    void setVoltageRegDirect(uint8_t voltReg);
    void setPower(DAC_PWR_ power);
};

#endif

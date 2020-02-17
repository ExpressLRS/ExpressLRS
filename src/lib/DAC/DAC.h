#pragma once

#ifdef TARGET_R9M_TX

#include "../../src/targets.h"
#include <wire.h>

typedef enum
{
    R9_PWR_10mw = 0,
    R9_PWR_25mw = 1,
    R9_PWR_50mw = 2,
    R9_PWR_100mw = 3,
    R9_PWR_250mw = 4,
    R9_PWR_500mw = 5,
    R9_PWR_1000mw = 6,
    R9_PWR_2000mw = 7,

} DAC_PWR_;

#define VCC 3300

class R9DAC
{
private:
    static int LUT[8][4];
    static uint8_t SDA;
    static uint8_t SCL;
    static uint8_t ADDR;

public:
    static uint32_t CurrVoltageMV;
    static uint8_t CurrVoltageRegVal;

    static void init(uint8_t SDA_, uint8_t SCL_, uint8_t ADDR_);

    static void setVoltageMV(uint32_t voltsMV);

    static void setVoltageRegDirect(uint8_t voltReg);

    static void setPower(DAC_PWR_ power);
};

#endif
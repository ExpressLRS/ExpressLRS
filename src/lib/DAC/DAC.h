#pragma once

#ifdef TARGET_R9M_TX

#include "POWERMGNT.h"
#include <stdint.h>

class R9DAC
{
private:
    enum
    {
        R9_PWR_10mW = 0,
        R9_PWR_25mW,
        R9_PWR_50mW,
        R9_PWR_100mW,
        R9_PWR_250mW,
        R9_PWR_500mW,
        R9_PWR_1000mW,
        R9_PWR_2000mW,
        R9_PWR_MAX
    };

    typedef struct
    {
        uint16_t mW;
        uint8_t dB;
        uint8_t gain;
        uint16_t volts; // APC2volts*1000
    } r9dac_lut_s;

    const r9dac_lut_s LUT[R9_PWR_MAX] = {
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

    enum
    {
        UNKNOWN = 0,
        RUNNING = 1,
        STANDBY = 2

    } DAC_State;

    uint32_t CurrVoltageMV;
    uint8_t CurrVoltageRegVal;
    uint8_t ADDR;
    int8_t pin_RFswitch;
    int8_t pin_RFamp;

    uint8_t get_lut_index(PowerLevels_e &power);

public:
    R9DAC();
    void init(uint8_t SDA_, uint8_t SCL_, uint8_t ADDR_, int8_t pin_switch = -1, int8_t pin_amp = -1);
    void standby();
    void resume();
    void setVoltageMV(uint32_t voltsMV);
    void setVoltageRegDirect(uint8_t voltReg);
    void setPower(PowerLevels_e &power);
};

#endif

#pragma once

#if defined(TARGET_R9M_TX) || defined(TARGET_TX_ES915TX)

#include "../../src/targets.h"
#include <Wire.h>

#if defined(Regulatory_Domain_AU_915) || defined(Regulatory_Domain_EU_868) || defined(Regulatory_Domain_FCC_915) || defined(Regulatory_Domain_AU_433) || defined(Regulatory_Domain_EU_433)
    #include "SX127xDriver.h"
    extern SX127xDriver Radio;
#elif defined(Regulatory_Domain_ISM_2400)
    #include "SX1280Driver.h"
    extern SX1280Driver Radio;
#endif

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

typedef enum
{
    UNKNOWN = 0,
    RUNNING = 1,
    STANDBY = 2
} DAC_STATE_;

//////////////////////////////////////////////////////

class DAC
{
public:
    void init();
    void standby();
    void resume();
    void setVoltageMV(uint32_t voltsMV);
    void setVoltageRegDirect(uint8_t voltReg);
    void setPower(DAC_PWR_ power);

private:
    static int  LUT[8][4];
    DAC_STATE_  m_state;
    uint32_t    m_currVoltageMV;
    uint8_t     m_currVoltageRegVal;
};

#endif
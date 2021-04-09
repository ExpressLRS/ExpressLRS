#pragma once

#if defined(TARGET_R9M_TX) || defined(TARGET_TX_ES915TX) || defined(TARGET_NAMIMNORC_VOYAGER_TX)

#include "targets.h"
#include <Wire.h>

#if DAC_IN_USE && defined(DAC_I2C_ADDRESS)

typedef enum
{
    DAC_PWR_10mW = 0,
    DAC_PWR_25mW = 1,
    DAC_PWR_50mW = 2,
    DAC_PWR_100mW = 3,
    DAC_PWR_250mW = 4,
    DAC_PWR_500mW = 5,
    DAC_PWR_1000mW = 6,
    DAC_PWR_2000mW = 7
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
    DAC_STATE_  m_state;
    uint32_t    m_currVoltageMV;
    uint8_t     m_currVoltageRegVal;
};

extern DAC TxDAC;
#endif // DAC_IN_USE && defined(DAC_I2C_ADDRESS)

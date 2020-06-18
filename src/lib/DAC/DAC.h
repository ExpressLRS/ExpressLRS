#pragma once

#ifdef TARGET_R9M_TX

#include "../../src/targets.h"
#include <Wire.h>

#if defined(Regulatory_Domain_AU_915) || defined(Regulatory_Domain_EU_868) || defined(Regulatory_Domain_FCC_915) || defined(Regulatory_Domain_AU_433) || defined(Regulatory_Domain_EU_433)
#include "SX127xDriver.h"
extern SX127xDriver Radio;
#endif

#if defined(Regulatory_Domain_ISM_2400)
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

#define VCC 3300

class R9DAC
{
private:
    static int LUT[8][4];
    static uint8_t SDA;
    static uint8_t SCL;
    static uint8_t ADDR;

public:
    static DAC_STATE_ DAC_STATE;
    static uint32_t CurrVoltageMV;
    static uint8_t CurrVoltageRegVal;

    static void init();
    static void standby();
    static void resume();
    static void setVoltageMV(uint32_t voltsMV);
    static void setVoltageRegDirect(uint8_t voltReg);
    static void setPower(DAC_PWR_ power);
};

#endif
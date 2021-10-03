#pragma once

#include "Arduino.h"
#include "targets.h"
#include "DAC.h"

#if defined(Regulatory_Domain_AU_915) || defined(Regulatory_Domain_EU_868) || defined(Regulatory_Domain_IN_866) || defined(Regulatory_Domain_FCC_915) || defined(Regulatory_Domain_AU_433) || defined(Regulatory_Domain_EU_433)
#include "SX127xDriver.h"
#elif Regulatory_Domain_ISM_2400
#include "SX1280Driver.h"
#endif

#if defined(TARGET_1000mW_MODULE) && !defined(POWER_VALUES)
#define MinPower PWR_50mW
#define MaxPower PWR_1000mW
#define POWER_VALUES {2,5,8,12,15}
#endif
#if !defined(POWER_VALUES)
#define MinPower PWR_10mW
#define MaxPower PWR_50mW
#define POWER_VALUES {8,11,15}
#endif

#if !defined(UNLOCK_HIGHER_POWER) && MaxPower > PWR_250mW
#undef MaxPower
#define MaxPower PWR_250mW
#endif

#if MinPower > PWR_50mW
#define DefaultPowerEnum MinPower
#elif MaxPower < PWR_50mW
#define DefaultPowerEnum MaxPower
#else
#define DefaultPowerEnum PWR_50mW
#endif

typedef enum
{
    PWR_10mW = 0,
    PWR_25mW = 1,
    PWR_50mW = 2,
    PWR_100mW = 3,
    PWR_250mW = 4,
    PWR_500mW = 5,
    PWR_1000mW = 6,
    PWR_2000mW = 7,
    PWR_COUNT = 8
} PowerLevels_e;

class POWERMGNT
{

private:
    static PowerLevels_e CurrentPower;

public:
    static PowerLevels_e setPower(PowerLevels_e Power);
    static PowerLevels_e incPower();
    static PowerLevels_e decPower();
    static PowerLevels_e currPower();
    static uint8_t powerToCrsfPower(PowerLevels_e Power);
    static void setDefaultPower();
    static void init();
};

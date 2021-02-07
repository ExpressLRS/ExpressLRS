#pragma once

#include "Arduino.h"
#include "../../src/targets.h"
#include "DAC.h"

#if defined(Regulatory_Domain_AU_915) || defined(Regulatory_Domain_EU_868) || defined(Regulatory_Domain_FCC_915) || defined(Regulatory_Domain_AU_433) || defined(Regulatory_Domain_EU_433)
#include "SX127xDriver.h"
#elif Regulatory_Domain_ISM_2400
#include "SX1280Driver.h"
#endif

#if defined(TARGET_R9M_TX) || defined(TARGET_TX_ES915TX)
#ifdef UNLOCK_HIGHER_POWER
#define MaxPower 6
#else
#define MaxPower 4
#endif
#define DefaultPowerEnum 2
#endif


#ifdef TARGET_TX_GHOST
#define MaxPower 4
#define DefaultPowerEnum 2
#endif

#ifdef TARGET_R9M_LITE_PRO_TX
#define MaxPower 6
#define DefaultPowerEnum 3
#endif

#ifdef TARGET_100mW_MODULE
#define MaxPower 2
#define DefaultPowerEnum 2
#endif

#ifdef TARGET_1000mW_MODULE
#define MaxPower 4
#define DefaultPowerEnum 2
#endif

#ifdef TARGET_R9M_LITE_TX
#define MaxPower 2
#define DefaultPowerEnum 2
#endif

#ifdef TARGET_TX_ESP32_SX1280_V1
#define MaxPower 0 // Output is actually 14mW
#define DefaultPowerEnum 0
#endif

#ifdef TARGET_TX_ESP32_E28_SX1280_V1
#define MaxPower 4
#define DefaultPowerEnum 2
#endif

#ifdef TARGET_TX_ESP32_LORA1280F27
#define MaxPower 4
#define DefaultPowerEnum 2
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
    PWR_2000mW = 7

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
    static void setDefaultPower();
    static void init();
};

#pragma once

#include "Arduino.h"
#include "targets.h"
#include "DAC.h"

#if defined(Regulatory_Domain_AU_915) || defined(Regulatory_Domain_EU_868) || defined(Regulatory_Domain_FCC_915) || defined(Regulatory_Domain_AU_433) || defined(Regulatory_Domain_EU_433)
#include "SX127xDriver.h"
#elif Regulatory_Domain_ISM_2400
#include "SX1280Driver.h"
#endif

#if defined(TARGET_1000mW_MODULE) || \
    defined(TARGET_R9M_TX)        || \
    defined(TARGET_TX_ES915TX)
#ifdef UNLOCK_HIGHER_POWER
#define MaxPower PWR_1000mW
#else
#define MaxPower PWR_250mW
#endif
#define DefaultPowerEnum PWR_50mW

#elif defined(TARGET_R9M_LITE_PRO_TX)
#define MaxPower PWR_1000mW
#define DefaultPowerEnum PWR_100mW

#elif defined(TARGET_TX_BETAFPV_2400_V1) || defined(TARGET_TX_BETAFPV_900_V1)
#define MaxPower PWR_500mW
#define DefaultPowerEnum PWR_50mW

#elif defined(TARGET_TX_ESP32_E28_SX1280_V1) || \
      defined(TARGET_TX_ESP32_LORA1280F27)   || \
      defined(TARGET_TX_GHOST)
#define MaxPower PWR_250mW
#define DefaultPowerEnum PWR_50mW

#elif defined(TARGET_TX_ESP32_SX1280_V1)
#define MaxPower PWR_10mW // Output is actually 14mW
#define DefaultPowerEnum PWR_10mW

#elif defined(TARGET_TX_FM30) || defined(TARGET_RX_FM30_MINI)
#define MaxPower PWR_100mW
#define DefaultPowerEnum PWR_50mW

#else
// Default is "100mW module"
//  ==> average ouput is 50mW with high duty cycle
#define MaxPower PWR_50mW
#define DefaultPowerEnum PWR_50mW
#ifndef TARGET_100mW_MODULE
#define TARGET_100mW_MODULE 1
#endif
#endif

#if !defined(MaxPower) && defined(TARGET_RX)
#define MaxPower PWR_2000mW
#define DefaultPowerEnum PWR_2000mW
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
    #if defined(TARGET_TX_BETAFPV_2400_V1) || defined(TARGET_TX_BETAFPV_900_V1)
        static void changePower();
    #endif
};

#pragma once

#include "Arduino.h"
#include "../../src/targets.h"
#include "LoRaRadioLib.h"
#include "DAC.h"

#ifdef TARGET_R9M_TX
#define MaxPower 7
#define MaxPowerEnum 6
#define DefaultPowerEnum 3
#elif defined(TARGET_100mW_MODULE)
#define MaxPower 2
#define MaxPowerEnum 2
#define DefaultPowerEnum 2
#elif defined(TARGET_1000mW_MODULE)
#define MaxPower 6
#define MaxPowerEnum 5
#define DefaultPowerEnum 3
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

// typedef enum
// {
//     PWR_10mW = 0,
//     PWR_25mW = 1,
//     PWR_50mW = 2,

// } SX127x_PA_Boost_PowerLevels_e;

class POWERMGNT
{

private:
    PowerLevels_e CurrentPower;
    PowerLevels_e setPower(PowerLevels_e Power);

public:
    POWERMGNT();
    PowerLevels_e incPower();
    PowerLevels_e decPower();
    PowerLevels_e currPower();
    void defaultPower();
};

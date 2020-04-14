#pragma once

#include "Arduino.h"
#include "LoRaRadioLib.h"

#ifdef TARGET_R9M_TX
//#define MaxPower PWR_2000mW        // 7
#define MaxPower PWR_1000mW        // 6
#define DefaultPowerEnum PWR_100mW // 3
#elif defined(TARGET_100mW_MODULE)
#define MaxPower PWR_50mW         // 2
#define DefaultPowerEnum PWR_50mW // 2
#elif defined(TARGET_1000mW_MODULE)
#define MaxPower PWR_1000mW        // 6
#define DefaultPowerEnum PWR_100mW // 3
#endif

typedef enum
{
    PWR_10mW = 0,
    PWR_25mW,
    PWR_50mW,
    PWR_100mW,
    PWR_250mW,
    PWR_500mW,
    PWR_1000mW,
    PWR_2000mW,
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

    void pa_off(void);
    void pa_on(void);
};

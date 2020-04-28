#pragma once

#include "LoRaRadioLib.h"

#ifdef TARGET_R9M_TX
#define MaxPower         PWR_1000mW // was PWR_2000mW
#define DefaultPowerEnum PWR_50mW   // was PWR_100mW
#elif defined(TARGET_1000mW_MODULE)
#define MaxPower         PWR_250mW // 4
#define DefaultPowerEnum PWR_50mW  // 2
#else
// TARGET_100mW_MODULE
#define MaxPower         PWR_50mW // 2
#define DefaultPowerEnum PWR_50mW // 2
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
    PWR_UNKNOWN
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
    void setPower(PowerLevels_e Power);

public:
    POWERMGNT();
    PowerLevels_e incPower();
    PowerLevels_e decPower();
    PowerLevels_e currPower();
    void defaultPower();

    uint8_t power_to_radio_enum(PowerLevels_e val = PWR_UNKNOWN)
    {
        if (val == PWR_UNKNOWN)
            val = CurrentPower;
        // ( enum 0mW = 0, 10mW, 25 mW, 100 mW, 500 mW, 1000 mW, 2000mW, 250mW )
        switch (val)
        {
            case PWR_10mW:
                return 1;
            case PWR_25mW:
                return 2;
            case PWR_50mW:
                return 2; // not speficied
            case PWR_100mW:
                return 3;
            case PWR_250mW:
                return 7;
            case PWR_500mW:
                return 4;
            case PWR_1000mW:
                return 5;
            case PWR_2000mW:
                return 6;
            case PWR_UNKNOWN:
            default:
                return 0;
        }
    }

    void pa_off(void);
    void pa_on(void);
};

#include "POWERMGNT.h"
#include "../../src/targets.h"

extern SX127xDriver Radio;
#ifdef TARGET_R9M_TX
extern R9DAC R9DAC;
#endif

PowerLevels_e POWERMGNT::CurrentPower = (PowerLevels_e)DefaultPowerEnum;

PowerLevels_e POWERMGNT::incPower()
{
    if (CurrentPower < MaxPower)
    {
        setPower((PowerLevels_e)((uint8_t)CurrentPower + 1));
    }
    return CurrentPower;
}

PowerLevels_e POWERMGNT::decPower()
{
    if (CurrentPower > 0)
    {
        setPower((PowerLevels_e)((uint8_t)CurrentPower - 1));
    }
    return CurrentPower;
}

PowerLevels_e POWERMGNT::currPower()
{
    return CurrentPower;
}

void POWERMGNT::init()
{
    #ifdef TARGET_R9M_TX
    Serial.println("Init TARGET_R9M_TX DAC Driver");
    //R9DAC.init();
    #endif
}

void POWERMGNT::setDefaultPower()
{
    setPower((PowerLevels_e)DefaultPowerEnum);
}


void POWERMGNT::setPower(PowerLevels_e Power)
{

#ifdef TARGET_R9M_TX
    Radio.SetOutputPower(0b0000);
    R9DAC.setPower((DAC_PWR_)Power);
    CurrentPower = Power;
#endif

#if defined(TARGET_100mW_MODULE) || defined(TARGET_R9M_LITE_TX)

    if (Power <= PWR_50mW)
    {
        if (Power == PWR_10mW)
        {
            Radio.SetOutputPower(0b1000);
            CurrentPower = PWR_10mW;
        }
        if (Power == PWR_25mW)
        {
            Radio.SetOutputPower(0b1100);
            CurrentPower = PWR_25mW;
        }
        if (Power == PWR_50mW)
        {
            Radio.SetOutputPower(0b1111); //15
            CurrentPower = PWR_50mW;
        }
    }

#endif

#ifdef TARGET_1000mW_MODULE
    switch (Power)
    {
    case PWR_100mW:
        Radio.SetOutputPower(0b0101);
        CurrentPower = PWR_100mW;
        break;
    case PWR_250mW:
        Radio.SetOutputPower(0b1000);
        CurrentPower = PWR_250mW;
        break;
    case PWR_500mW:
        Radio.SetOutputPower(0b1100);
        CurrentPower = PWR_500mW;
        break;
    case PWR_1000mW:
        Radio.SetOutputPower(0b1111);
        CurrentPower = PWR_1000mW;
        break;
    case PWR_50mW:
    default:
        Radio.SetOutputPower(0b0010);
        CurrentPower = PWR_50mW;
        break;
    }
#endif
}

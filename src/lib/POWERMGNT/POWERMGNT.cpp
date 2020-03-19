#include "POWERMGNT.h"

extern SX127xDriver Radio;
#ifdef PLATFORM_R9M_TX
extern R9DAC R9DAC;
#endif

PowerLevels_e POWERMGNT::CurrentPower = (PowerLevels_e)DefaultPowerEnum;

PowerLevels_e POWERMGNT::incPower()
{
    if (CurrentPower < MaxPower)
    {
        CurrentPower = (PowerLevels_e)((uint8_t)CurrentPower + 1);
        setPower(CurrentPower);
    }
    return CurrentPower;
}

PowerLevels_e POWERMGNT::decPower()
{
    if (CurrentPower > 0)
    {
        CurrentPower = (PowerLevels_e)((uint8_t)CurrentPower - 1);
        setPower(CurrentPower);
    }
    return CurrentPower;
}

PowerLevels_e POWERMGNT::currPower()
{
    return CurrentPower;
}

void POWERMGNT::defaultPower()
{
    setPower((PowerLevels_e)DefaultPowerEnum);
}

void POWERMGNT::setPower(PowerLevels_e Power)
{

#ifdef TARGET_R9M_TX
    Radio.SetOutputPower(0b0000);
    //R9DAC.setPower((DAC_PWR_)Power);
#endif

#ifdef TARGET_100mW_MODULE

    if (Power == PWR_10mW)
    {
        Radio.SetOutputPower(0b1000);
    }
    if (Power == PWR_25mW)
    {
        Radio.SetOutputPower(0b1100);
    }
    if (Power == PWR_50mW)
    {
        Radio.SetOutputPower(0b1111); //15
    }

#endif

#ifdef TARGET_1000mW_MODULE
    Radio.SetOutputPower(0b0000);
// not done yet
#endif
}
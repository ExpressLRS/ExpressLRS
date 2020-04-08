#include "POWERMGNT.h"

#ifdef TARGET_R9M_TX
class R9DAC;
extern R9DAC r9dac;
#endif

extern SX127xDriver Radio;

POWERMGNT::POWERMGNT()
{
    CurrentPower = (PowerLevels_e)DefaultPowerEnum;
}

PowerLevels_e
POWERMGNT::incPower()
{
    if (CurrentPower < MaxPower)
    {
        CurrentPower = setPower((PowerLevels_e)((uint8_t)CurrentPower + 1));
    }
    return CurrentPower;
}

PowerLevels_e POWERMGNT::decPower()
{
    if (CurrentPower > 0)
    {
        CurrentPower = setPower((PowerLevels_e)((uint8_t)CurrentPower - 1));
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

PowerLevels_e POWERMGNT::setPower(PowerLevels_e Power)
{
#ifdef TARGET_R9M_TX
    Radio.SetOutputPower(0b0000);
    r9dac.setPower((DAC_PWR_)Power);
    return Power;

#elif defined(TARGET_100mW_MODULE)
    if (Power <= PWR_50mW)
    {
        if (Power == PWR_10mW)
        {
            Radio.SetOutputPower(0b1000);
            return PWR_10mW;
        }
        if (Power == PWR_25mW)
        {
            Radio.SetOutputPower(0b1100);
            return PWR_25mW;
        }
        if (Power == PWR_50mW)
        {
            Radio.SetOutputPower(0b1111); //15
            return PWR_50mW;
        }
    }
    return CurrentPower;

#elif defined(TARGET_1000mW_MODULE)
    Radio.SetOutputPower(0b0000);
    return PWR_25mW;
// not done yet
#else
    return 0;
#endif
}

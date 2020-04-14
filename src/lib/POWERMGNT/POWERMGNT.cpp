#include "POWERMGNT.h"

#ifdef TARGET_R9M_TX
#include "DAC.h"
extern R9DAC r9dac;
#endif

extern SX127xDriver Radio;

POWERMGNT::POWERMGNT()
{
    CurrentPower = DefaultPowerEnum;
}

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
    if (CurrentPower > PWR_10mW)
    {
        setPower((PowerLevels_e)((uint8_t)CurrentPower - 1));
    }
    return CurrentPower;
}

PowerLevels_e POWERMGNT::currPower()
{
    return CurrentPower;
}

void POWERMGNT::defaultPower()
{
    setPower(DefaultPowerEnum);
}

PowerLevels_e POWERMGNT::setPower(PowerLevels_e Power)
{
    if (Power == CurrentPower || Power < PWR_10mW ||
        Power > MaxPower)
        return CurrentPower;

#ifdef TARGET_R9M_TX
    r9dac.setPower(Power);

#elif defined(TARGET_100mW_MODULE)
    switch (Power)
    {
    case PWR_10mW:
        Radio.SetOutputPower(0b1000);
        break;
    case PWR_25mW:
        Radio.SetOutputPower(0b1100);
        break;
    case PWR_50mW:
        Radio.SetOutputPower(0b1111);
        break;
    default:
        Power = CurrentPower;
        break;
    }
#elif defined(TARGET_1000mW_MODULE)
    Radio.SetOutputPower(0b0000);
    Power = PWR_25mW;
#else
    Power = 0;
#endif

    CurrentPower = Power;
    return Power;
}

void POWERMGNT::pa_off(void)
{
#ifdef TARGET_R9M_TX
    r9dac.standby();
#endif
}

void POWERMGNT::pa_on(void)
{
#ifdef TARGET_R9M_TX
    r9dac.resume();
#endif
}

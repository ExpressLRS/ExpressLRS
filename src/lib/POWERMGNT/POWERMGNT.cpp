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

void POWERMGNT::setPower(PowerLevels_e Power)
{
    if (Power == CurrentPower || Power < PWR_10mW ||
        Power > MaxPower)
        return;

#ifdef TARGET_R9M_TX
    Radio.SetOutputPower(0b1000);
    r9dac.setPower(Power);

#elif defined(TARGET_1000mW_MODULE)
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

#else
    // TARGET_100mW_MODULE
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
#endif
    CurrentPower = Power;
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

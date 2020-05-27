#include "POWERMGNT.h"

#ifdef TARGET_R9M_TX
#include "DAC.h"
extern R9DAC r9dac;
#endif

POWERMGNT::POWERMGNT(SX127xDriver &radio)
    : p_radio(radio), p_current_power(PWR_UNKNOWN)
{
}

void POWERMGNT::Begin()
{
#ifdef TARGET_R9M_TX
    p_radio.SetOutputPower(0b0000);
#endif
}

PowerLevels_e POWERMGNT::incPower()
{
    if (p_dyn_power && p_current_power < MaxPower)
    {
        p_set_power((PowerLevels_e)((uint8_t)p_current_power + 1));
    }
    return p_current_power;
}

PowerLevels_e POWERMGNT::decPower()
{
    if (p_dyn_power && p_current_power > PWR_10mW)
    {
        p_set_power((PowerLevels_e)((uint8_t)p_current_power - 1));
    }
    return p_current_power;
}

PowerLevels_e POWERMGNT::loopPower()
{
    PowerLevels_e next;
    if (p_dyn_power)
    {
        next = PWR_10mW;
    }
    else
    {
        next = (PowerLevels_e)((p_current_power + 1) % (MaxPower+1));
    }
    setPower(next);
    return next;
}

void POWERMGNT::setPower(PowerLevels_e power)
{
    if (power == PWR_DYNAMIC)
    {
        // enable dynamic power and reset current level to default
        p_dyn_power = 1;
        power = TX_POWER_DEFAULT;
    }
    else
    {
        if (power == PWR_UNKNOWN)
            power = TX_POWER_DEFAULT;
        p_dyn_power = 0;
    }

    p_set_power(power);
}

void POWERMGNT::pa_off(void) const
{
#ifdef TARGET_R9M_TX
    r9dac.standby();
#endif
}

void POWERMGNT::pa_on(void) const
{
#ifdef TARGET_R9M_TX
    r9dac.resume();
#endif
}

/************************** PRIVATE ******************************/

void POWERMGNT::p_set_power(PowerLevels_e power)
{
    if (power == p_current_power || power < PWR_10mW ||
        power > MaxPower)
        return;

#ifdef TARGET_R9M_TX
    r9dac.setPower(power);

#elif defined(TARGET_1000mW_MODULE)
    switch (power)
    {
        case PWR_100mW:
            p_radio.SetOutputPower(0b0101);
            break;
        case PWR_250mW:
            p_radio.SetOutputPower(0b1000);
            break;
        case PWR_500mW:
            p_radio.SetOutputPower(0b1100);
            break;
        case PWR_1000mW:
            p_radio.SetOutputPower(0b1111);
            break;
        default:
            p_radio.SetOutputPower(0b0010);
            p_current_power = PWR_50mW;
            break;
    }

#else
    // TARGET_100mW_MODULE
    switch (power)
    {
        case PWR_10mW:
            p_radio.SetOutputPower(0b1000);
            break;
        case PWR_25mW:
            p_radio.SetOutputPower(0b1100);
            break;
        case PWR_50mW:
            p_radio.SetOutputPower(0b1111);
            break;
        default:
            power = p_current_power;
            break;
    }
#endif
    p_current_power = power;
}

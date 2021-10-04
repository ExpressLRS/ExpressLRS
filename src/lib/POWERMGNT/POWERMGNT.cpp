#include "POWERMGNT.h"
#include "DAC.h"
#include "targets.h"


#if defined(Regulatory_Domain_AU_915) || defined(Regulatory_Domain_EU_868) || defined(Regulatory_Domain_IN_866) || defined(Regulatory_Domain_FCC_915) || defined(Regulatory_Domain_AU_433) || defined(Regulatory_Domain_EU_433)
extern SX127xDriver Radio;
#elif Regulatory_Domain_ISM_2400
extern SX1280Driver Radio;
#endif

PowerLevels_e POWERMGNT::CurrentPower = PWR_COUNT; // default "undefined" initial value
#if defined(POWER_VALUES)
static int16_t powerValues[] = POWER_VALUES;
#endif

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
    if (CurrentPower > MinPower)
    {
        setPower((PowerLevels_e)((uint8_t)CurrentPower - 1));
    }
    return CurrentPower;
}

PowerLevels_e POWERMGNT::currPower()
{
    return CurrentPower;
}

uint8_t POWERMGNT::powerToCrsfPower(PowerLevels_e Power)
{
    // Crossfire's power levels as defined in opentx:radio/src/telemetry/crossfire.cpp
    //static const int32_t power_values[] = { 0, 10, 25, 100, 500, 1000, 2000, 250, 50 };
    switch (Power)
    {
    case PWR_10mW: return 1;
    case PWR_25mW: return 2;
    case PWR_50mW: return 8;
    case PWR_100mW: return 3;
    case PWR_250mW: return 7;
    case PWR_500mW: return 4;
    case PWR_1000mW: return 5;
    case PWR_2000mW: return 6;
    default:
        return 0;
    }
}

void POWERMGNT::init()
{
#if defined(DAC_IN_USE)
    TxDAC.init();
#elif defined(POWER_OUTPUT_ANALOG)
    //initialize both 12 bit DACs
    pinMode(GPIO_PIN_RFamp_APC1, OUTPUT);
    pinMode(GPIO_PIN_RFamp_APC2, OUTPUT);
    analogWriteResolution(12);

    // WARNING: The two calls to analogWrite below are needed for the
    // lite pro, as it seems that the very first calls to analogWrite
    // fail for an unknown reason (suspect Arduino lib bug). These
    // set power to 50mW, which should get overwitten shortly after
    // boot by whatever is set in the EEPROM. @wvarty
    analogWrite(GPIO_PIN_RFamp_APC1, 3350); //0-4095 2.7V
    analogWrite(GPIO_PIN_RFamp_APC2, 950);
#endif
#if defined(GPIO_PIN_FAN_EN) && (GPIO_PIN_FAN_EN != UNDEF_PIN)
    pinMode(GPIO_PIN_FAN_EN, OUTPUT);
#endif
    setDefaultPower();
}

void POWERMGNT::setDefaultPower()
{
    setPower(DefaultPowerEnum);
}

PowerLevels_e POWERMGNT::setPower(PowerLevels_e Power)
{
    if (Power == CurrentPower)
        return CurrentPower;

    if (Power < MinPower)
    {
        Power = MinPower;
    }
    else if (Power > MaxPower)
    {
        Power = MaxPower;
    }

#ifdef GPIO_PIN_FAN_EN
    digitalWrite(GPIO_PIN_FAN_EN, (Power >= PWR_250mW) ? HIGH : LOW);
#endif

#if DAC_IN_USE
    // DAC is used e.g. for R9M, ES915TX and Voyager
    Radio.SetOutputPower(0b0000);
    TxDAC.setPower(powerValues[Power - MinPower]);
#elif defined(POWER_OUTPUT_ANALOG)
    Radio.SetOutputPower(0b0000);
    //Set DACs PA5 & PA4
    analogWrite(GPIO_PIN_RFamp_APC1, 3350); //0-4095 2.7V
    analogWrite(GPIO_PIN_RFamp_APC2, powerValues[Power - MinPower]);
#elif defined(POWER_OUTPUT_DACWRITE)
    Radio.SetOutputPower(0b0000);
    dacWrite(GPIO_PIN_RFamp_APC2, powerValues[Power - MinPower]);
#elif defined(POWER_VALUES)
    Radio.SetOutputPower(powerValues[Power - MinPower]);
#elif defined(TARGET_RX)
    #if defined(TARGET_RX_DEFAULT_POWER)
        Radio.SetOutputPower(TARGET_RX_DEFAULT_POWER);
    #else
        // Set to max power for telemetry on the RX if not specified
        Radio.SetOutputPowerMax();
    #endif
#else
#error "[ERROR] Unknown power management!"
#endif
    CurrentPower = Power;
    return Power;
}

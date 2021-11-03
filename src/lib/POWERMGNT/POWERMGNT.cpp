#include "POWERMGNT.h"
#include "DAC.h"

/*
 * Moves the power management values and special cases out of the main code and into `targets.h`.
 *
 * TX Targets
 * ***********
 *
 * A new target now just needs to define
 * - `MinPower`, the minimum power level supported
 * - `MaxPower`, the absolute maximum power level supported
 * - optionally `HighPower`, if defined then module uses this as max unless `UNLOCK_HIGHER_POWER` is defined by the user
 * - `POWER_OUTPUT_VALUES` array of values to be used to set appropriate power level from `MinPower` to `MaxPower`
 *
 * A target can also define one of the following to configure how the output power is set, the value given to the function
 * is the value from the `POWER_OUTPUT_VALUES` array.
 *
 * - `POWER_OUTPUT_DAC` to use i2c DAC specify the address of the DAC in this define
 * - `POWER_OUTPUT_DACWRITE` to use `dacWrite` function
 * - `POWER_OUTPUT_ANALOG` to use `analogWrite` function
 * - `POWER_OUTPUT_FIXED` which will provide the value to `Radio.SetOutputPower` function
 * - default is to use `Radio.SetOutputPower` function
 *
 * RX Targets
 * **********
 *
 * Can define `POWER_OUTPUT_FIXED` which will provide the value to `Radio.SetOutputPower` function.
 * If nothing is defined then the default method `Radio.SetOutputPowerMax` will be used, which sets the value to
 * 13 on SX1280 (~12.5dBm) or 15 on SX127x (~17dBm)
 */

#if defined(Regulatory_Domain_AU_915) || defined(Regulatory_Domain_EU_868) || defined(Regulatory_Domain_IN_866) || defined(Regulatory_Domain_FCC_915) || defined(Regulatory_Domain_AU_433) || defined(Regulatory_Domain_EU_433)
extern SX127xDriver Radio;
#elif Regulatory_Domain_ISM_2400
extern SX1280Driver Radio;
#endif

PowerLevels_e POWERMGNT::CurrentPower = PWR_COUNT; // default "undefined" initial value
#if defined(POWER_OUTPUT_VALUES)
static int16_t powerValues[] = POWER_OUTPUT_VALUES;
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
#if defined(POWER_OUTPUT_DAC)
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

PowerLevels_e POWERMGNT::getDefaultPower()
{
    if (MinPower > PWR_50mW)
    {
        return MinPower;
    }
    if (MaxPower < PWR_50mW)
    {
        return MaxPower;
    }
    return PWR_50mW;
}

void POWERMGNT::setDefaultPower()
{
    setPower(getDefaultPower());
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

#if defined(POWER_OUTPUT_DAC)
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
#elif defined(POWER_OUTPUT_FIXED)
    Radio.SetOutputPower(POWER_OUTPUT_FIXED);
#elif defined(POWER_OUTPUT_VALUES) && defined(TARGET_TX)
    Radio.SetOutputPower(powerValues[Power - MinPower]);
#elif defined(TARGET_RX)
    // Set to max power for telemetry on the RX if not specified
    Radio.SetOutputPowerMax();
#else
#error "[ERROR] Unknown power management!"
#endif
    CurrentPower = Power;
    return Power;
}

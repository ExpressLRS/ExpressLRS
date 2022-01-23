#include "common.h"
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
PowerLevels_e POWERMGNT::FanEnableThreshold = PWR_250mW;
int8_t POWERMGNT::CurrentSX1280Power = 0;

#if defined(POWER_OUTPUT_VALUES)
static int16_t powerValues[] = POWER_OUTPUT_VALUES;
#endif

static int8_t powerCaliValues[PWR_COUNT] = {0};

#if defined(PLATFORM_ESP32)
nvs_handle POWERMGNT::handle = 0;
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

void POWERMGNT::incSX1280Ouput()
{
    if (CurrentSX1280Power < 13)
    {
        CurrentSX1280Power++;
        Radio.SetOutputPower(CurrentSX1280Power);
    }
}

void POWERMGNT::decSX1280Ouput()
{
    if (CurrentSX1280Power > -18)
    {
        CurrentSX1280Power--;
        Radio.SetOutputPower(CurrentSX1280Power);
    }
}

int8_t POWERMGNT::currentSX1280Ouput()
{
    return CurrentSX1280Power;
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

uint8_t POWERMGNT::getPowerIndBm()
{
    switch (CurrentPower)
    {
    case PWR_10mW: return 10;
    case PWR_25mW: return 14;
    case PWR_50mW: return 17;
    case PWR_100mW: return 20;
    case PWR_250mW: return 24;
    case PWR_500mW: return 27;
    case PWR_1000mW: return 30;
    case PWR_2000mW: return 33;
    default:
        return 0;
    }
}

void POWERMGNT::SetPowerCaliValues(int8_t *values, size_t size)
{
    bool isUpdate = false;
    for(size_t i=0 ; i<size ; i++)
    {
        if(powerCaliValues[i] != values[i])
        {
            powerCaliValues[i] = values[i];
            isUpdate = true;
        }
    }
#if defined(PLATFORM_ESP32)
    if (isUpdate)
    {
        nvs_set_blob(handle, "powercali", &powerCaliValues, sizeof(powerCaliValues));
    }
    nvs_commit(handle);
#else
    UNUSED(isUpdate);
#endif
}

void POWERMGNT::GetPowerCaliValues(int8_t *values, size_t size)
{
    for(size_t i=0 ; i<size ; i++)
    {
        *(values + i) = powerCaliValues[i];
    }
}

void POWERMGNT::LoadCalibration()
{
#if defined(PLATFORM_ESP32)
    // Initialize NVS
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK( err );
    ESP_ERROR_CHECK(nvs_open("PWRCALI", NVS_READWRITE, &handle));

    uint32_t version;
    if(nvs_get_u32(handle, "calversion", &version) != ESP_ERR_NVS_NOT_FOUND
        && version == (uint32_t)(CALIBRATION_VERSION | CALIBRATION_MAGIC))
    {
        size_t size = sizeof(powerCaliValues);
        nvs_get_blob(handle, "powercali", &powerCaliValues, &size);
    }
    else
    {
        nvs_set_blob(handle, "powercali", &powerCaliValues, sizeof(powerCaliValues));
        nvs_set_u32(handle, "calversion", CALIBRATION_VERSION | CALIBRATION_MAGIC);
        nvs_commit(handle);
    }
#else
    memset(powerCaliValues, 0, sizeof(powerCaliValues));
#endif
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
#if defined(GPIO_PIN_FAN_EN)
    pinMode(GPIO_PIN_FAN_EN, OUTPUT);
#endif
    LoadCalibration();
    setDefaultPower();
}

PowerLevels_e POWERMGNT::getDefaultPower()
{
    if (MinPower > DefaultPower)
    {
        return MinPower;
    }
    if (MaxPower < DefaultPower)
    {
        return MaxPower;
    }
    return DefaultPower;
}

void POWERMGNT::setDefaultPower()
{
    setPower(getDefaultPower());
}

void POWERMGNT::setPower(PowerLevels_e Power)
{
    if (Power == CurrentPower)
        return;

    if (Power < MinPower)
    {
        Power = MinPower;
    }
    else if (Power > MaxPower)
    {
        Power = MaxPower;
    }

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
    CurrentSX1280Power = powerValues[Power - MinPower] + powerCaliValues[Power];
    Radio.SetOutputPower(CurrentSX1280Power);
#elif defined(TARGET_RX)
    // Set to max power for telemetry on the RX if not specified
    Radio.SetOutputPowerMax();
#else
#error "[ERROR] Unknown power management!"
#endif
    CurrentPower = Power;
    devicesTriggerEvent();
}

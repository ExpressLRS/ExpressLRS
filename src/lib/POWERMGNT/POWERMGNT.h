#pragma once

#include "options.h"

#if defined(PLATFORM_ESP32)
#include <nvs_flash.h>
#include <nvs.h>
#endif

#ifndef POWER_OUTPUT_VALUES
    // These are "fake" values as the power on the RX is not user selectable
    #define MinPower PWR_10mW
    #define MaxPower PWR_10mW
#endif

#if !defined(DefaultPower)
    #define DefaultPower PWR_50mW
#endif

#if !defined(HighPower)
#define HighPower MaxPower
#endif

#ifdef USE_LOW_POWER_TXEN_DISABLE
    #ifndef OPT_USE_LOW_POWER_TXEN_DISABLE
        #define OPT_USE_LOW_POWER_TXEN_DISABLE false
    #endif
#else
    #define OPT_USE_LOW_POWER_TXEN_DISABLE false
#endif

typedef enum
{
    PWR_10mW = 0,
    PWR_25mW = 1,
    PWR_50mW = 2,
    PWR_100mW = 3,
    PWR_250mW = 4,
    PWR_500mW = 5,
    PWR_1000mW = 6,
    PWR_2000mW = 7,
    PWR_COUNT = 8,
    PWR_MATCH_TX = PWR_COUNT,
} PowerLevels_e;

uint8_t powerToCrsfPower(PowerLevels_e Power);
PowerLevels_e crsfpowerToPower(uint8_t crsfpower);

class PowerLevelContainer
{
protected:
    static PowerLevels_e CurrentPower;
public:
    static PowerLevels_e currPower() { return CurrentPower; }
};

#ifndef UNIT_TEST

class POWERMGNT : public PowerLevelContainer
{

private:
    static int8_t CurrentSX1280Power;
    static PowerLevels_e FanEnableThreshold;
#if defined(PLATFORM_ESP32)
    static nvs_handle  handle;
#endif
    static void LoadCalibration();

public:
    /**
     * @brief Set the power level, constrained to MinPower..MaxPower
     *
     * @param Power the power level to set
     */
    static void setPower(PowerLevels_e Power);

    /**
     * @brief Increment to the next higher power level, capped at MaxPower
     *
     * @return PowerLevels_e the new power level
     */
    static PowerLevels_e incPower();

    /**
     * @brief Decrement to the next lower power level, capped at MinPower
     *
     * @return PowerLevels_e the new power level
     */
    static PowerLevels_e decPower();

    /**
     * @brief Get the currently selected power level
     *
     * @return PowerLevels_e the currently selected power level
     */
    static PowerLevels_e currPower() { return CurrentPower; }

    /**
     * @brief Get the MinPower level supported by this device
     *
     * @return PowerLevels_e the minimum power level supported
     */
    static PowerLevels_e getMinPower() { return MinPower; }

    /**
     * @brief Get the MaxPower level supported by this device.
     * For devices that support the HighPower override, i.e. R9M with the fan hack,
     * the MaxPower is normally HighPower unless the 'unlock_higher_power' option
     * is set at compile time.
     *
     * @return PowerLevels_e the maximum power level supported
     */
    static PowerLevels_e getMaxPower() {
        PowerLevels_e power;
        #if defined(TARGET_RX)
            power = MaxPower;
        #else
            power = firmwareOptions.unlock_higher_power ? MaxPower : HighPower;
        #endif
        #if defined(Regulatory_Domain_EU_CE_2400)
            if (power > PWR_100mW)
            {
                power = PWR_100mW;
            }
        #endif
        return power;
    }

    /**
     * @brief Get the Default power level for this device
     *
     * @return PowerLevels_e the default power level
     */
    static PowerLevels_e getDefaultPower();

    /**
     * @brief Set the output power to the default power level
     */
    static void setDefaultPower();

    /**
     * @brief Get the currently configured power level in dBm
     *
     * @return uint8_t the dBm for the current power level
     */
    static uint8_t getPowerIndBm();

    /**
     * @brief increment the SX1280 power level by 1 dBm, capped to 3dBm above the selected power level
     */
    static void incSX1280Output();

    /**
     * @brief decrement the SX1280 power level by 1 dBm, capped to 3dBm below the selected power level
     */
    static void decSX1280Output();

    /**
     * @brief Get the current value given to the SX1280 for it's output power.
     * This value may have been adjusted up/dowm from nominal by the PDET routine, if supported
     * by the device (i.e. modules with SKY85321 PA/LNA)
     *
     * @return int8_t the current (adjusted) SX1280 power level
     */
    static int8_t currentSX1280Output();

    /**
     * @brief Initialise the power management subsystem.
     * Configures PWM ouptut pins, DACs, loads power calibration settings
     * and sets output power to the default power level as appropriate for the
     * device
     */
    static void init();

    static void SetPowerCaliValues(int8_t *values, size_t size);
    static void GetPowerCaliValues(int8_t *values, size_t size);
};


#define CALIBRATION_MAGIC    0x43414C << 8   //['C', 'A', 'L']
#define CALIBRATION_VERSION   1

#endif /* !UNIT_TEST */

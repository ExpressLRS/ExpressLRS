#pragma once

#include "options.h"
#include "PowerLimits.h"

#if defined(PLATFORM_ESP32)
#include <nvs_flash.h>
#include <nvs.h>
#endif

uint8_t powerToCrsfPower(PowerLevels_e Power);
PowerLevels_e crsfPowerToPower(uint8_t crsfpower);

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
    static PowerRequest RequestedPower;
    static int8_t CurrentSX1280Power;
    static PowerLevels_e FanEnableThreshold;
#if defined(PLATFORM_ESP32)
    static nvs_handle  handle;
#endif
    static void LoadCalibration();

public:
    /**
     * @brief Set power within hardware limits and the active domain's output cap
     *
     * @param Power the power level to set
     */
    static void setPower(PowerLevels_e Power);

    // Re-evaluate the last request after selecting a different RF band.
    static void reapplyPower();

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
     * @brief Get the selectable maximum across this device's configured domains.
     *
     * @return PowerLevels_e the maximum power level supported
     */
    static PowerLevels_e getMaxPower();

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
};


#define CALIBRATION_MAGIC    0x43414C << 8   //['C', 'A', 'L']
#define CALIBRATION_VERSION   1

#endif /* !UNIT_TEST */

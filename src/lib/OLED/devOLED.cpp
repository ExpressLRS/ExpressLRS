#include "targets.h"
#include "common.h"
#include "device.h"

#if defined(USE_OLED_SPI) || defined(USE_OLED_SPI_SMALL) || defined(USE_OLED_I2C)

#include "POWERMGNT.h"
#include "OLED.h"

static int start()
{
    OLED::displayLogo();
    return 1000;    // set callback in 1s
}

static int timeout()
{
    static PowerLevels_e lastPower;
    static expresslrs_RFrates_e lastRate;
    static expresslrs_tlm_ratio_e lastRatio;
    const char thisCommit[] = {LATEST_COMMIT, 0};

    PowerLevels_e newPower = POWERMGNT::currPower();
    expresslrs_RFrates_e newRate = ExpressLRS_currAirRate_Modparams->enum_rate;
    expresslrs_tlm_ratio_e newRatio = ExpressLRS_currAirRate_Modparams->TLMinterval;

    if (lastPower != newPower || lastRate != newRate || lastRatio != newRatio)
    {
        OLED::updateScreen(newPower, newRate, newRatio, thisCommit);
        lastPower = newPower;
        lastRate = newRate;
        lastRatio = newRatio;
    }

    return 300; // check for updates every 300ms
}

device_t OLED_device = {
    .initialize = nullptr,
    .start = start,
    .event = nullptr,
    .timeout = timeout
};

#else

device_t OLED_device = {
    NULL
};

#endif

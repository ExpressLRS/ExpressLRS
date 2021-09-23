#if defined(TARGET_TX) 

#include "targets.h"
#include "common.h"
#include "device.h"

#if defined(HAS_OLED)

#include "POWERMGNT.h"

#include "OLED.h"
class OLED OLED;

static const char thisCommit[] = {LATEST_COMMIT, 0};
static unsigned long startupTime = 0;

static void initializeOLED()
{
    OLED.displayLogo();
    startupTime = millis();
}

static bool updateOLED(bool eventFired, unsigned long now)
{
    if (now - startupTime < 1000)
    {
        return false;
    }
    if (eventFired || startupTime != 0)
    {
        startupTime = 0;
        OLED.updateScreen(OLED.getPowerString((PowerLevels_e)POWERMGNT::currPower()),
                            OLED.getRateString((expresslrs_RFrates_e)ExpressLRS_currAirRate_Modparams->enum_rate),
                            OLED.getTLMRatioString((expresslrs_tlm_ratio_e)(ExpressLRS_currAirRate_Modparams->TLMinterval)), thisCommit);
    }
    return false;
}

device_t OLED_device = {
    initializeOLED,
    updateOLED
};

#else

device_t OLED_device = {
    NULL,
    NULL
};

#endif

#endif
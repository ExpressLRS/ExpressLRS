#include "targets.h"
#include "common.h"
#include "device.h"

#if defined(HAS_OLED)

#include "POWERMGNT.h"

#include "OLED.h"
class OLED OLED;

static const char thisCommit[] = {LATEST_COMMIT, 0};
static bool startup = false;

static int start()
{
    OLED.displayLogo();
    startup = true;
    return 1000;    // set callback in 1s
}

static int event(std::function<void ()> sendSpam)
{
    if (startup)
    {
        return DURATION_IGNORE; // we are still displaying the startup message, so don't change the timeout
    }
    OLED.updateScreen(OLED.getPowerString((PowerLevels_e)POWERMGNT::currPower()),
                        OLED.getRateString((expresslrs_RFrates_e)ExpressLRS_currAirRate_Modparams->enum_rate),
                        OLED.getTLMRatioString((expresslrs_tlm_ratio_e)(ExpressLRS_currAirRate_Modparams->TLMinterval)), thisCommit);
    return DURATION_NEVER;
}

static int timeout(std::function<void ()> sendSpam)
{
    startup = false;
    return event(sendSpam);
}

device_t OLED_device = {
    .initialize = NULL,
    .start = start,
    .event = event,
    .timeout = timeout
};

#else

device_t OLED_device = {
    NULL
};

#endif

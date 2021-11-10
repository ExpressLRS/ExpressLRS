#include "targets.h"
#include "common.h"
#include "device.h"
#include "config.h"

#if defined(USE_OLED_SPI) || defined(USE_OLED_SPI_SMALL) || defined(USE_OLED_I2C)

#include "POWERMGNT.h"

#include "OLED.h"
class OLED OLED;

extern TxConfig config;

static const char thisCommit[] = {LATEST_COMMIT, 0};
static bool startup = false;

static int start()
{
    OLED.displayLogo();
    startup = true;
    return 1000;    // set callback in 1s
}

static int event()
{
    if (startup)
    {
        return DURATION_IGNORE; // we are still displaying the startup message, so don't change the timeout
    }
    OLED.updateScreen(OLED.getPowerString((PowerLevels_e)(config.GetPower())),
                        OLED.getRateString((expresslrs_RFrates_e)(config.GetRate())),
                        OLED.getTLMRatioString((expresslrs_tlm_ratio_e)(config.GetTlm())), thisCommit);
    return DURATION_NEVER;
}

static int timeout()
{
    startup = false;
    return event();
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

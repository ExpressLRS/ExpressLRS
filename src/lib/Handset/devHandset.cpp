#include "targets.h"

#ifdef TARGET_TX

#include "CRSFHandset.h"
#include "POWERMGNT.h"
#include "devHandset.h"

#include "CRSFEndpoint.h"

#if defined(PLATFORM_ESP32)
#include "AutoDetect.h"
#endif

Handset *handset;

static bool initialize()
{
#if defined(PLATFORM_ESP32)
    if (GPIO_PIN_RCSIGNAL_RX == GPIO_PIN_RCSIGNAL_TX)
    {
        handset = new AutoDetect();
        return true;
    }
#endif
    handset = new CRSFHandset();
    return true;
}

static int start()
{
    handset->Begin();
#if defined(DEBUG_TX_FREERUN)
    handset->forceConnection();
#endif
    return DURATION_IMMEDIATELY;
}

static int timeout()
{
    handset->handleInput();
    return DURATION_IMMEDIATELY;
}

static int event()
{
    // An event should be generated every time the TX power changes
    //FIXME this should not be here either!
    crsfEndpoint->linkStats.uplink_TX_Power = powerToCrsfPower(PowerLevelContainer::currPower());
    return DURATION_IGNORE;
}

device_t Handset_device = {
    .initialize = initialize,
    .start = start,
    .event = event,
    .timeout = timeout,
    .subscribe = EVENT_POWER_CHANGED
};
#endif

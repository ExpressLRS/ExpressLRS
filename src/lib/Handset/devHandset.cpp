#include "targets.h"

#ifdef TARGET_TX

#include "CRSFHandset.h"
#include "SBUSHandset.h"
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
    // Check if SBUS mode is enabled via hardware configuration
    // If GPIO_PIN_RCSIGNAL_RX is set to a different pin than the default CRSF pin,
    // or if a specific SBUS flag is set, use SBUSHandset
    int sbusPin = hardware_pin(HARDWARE_serial1_rx);
    
    if (sbusPin != -1 && sbusPin != UNDEF_PIN)
    {
        DBGLN("SBUS mode enabled on pin %d", sbusPin);
        handset = new SBUSHandset();
        return true;
    }
    
    // Auto-detect between CRSF and PPM if using half-duplex pin
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

device_t Handset_device = {
    .initialize = initialize,
    .start = start,
    .event = nullptr,
    .timeout = timeout,
    .subscribe = EVENT_NONE,
};
#endif

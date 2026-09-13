#include "targets.h"

#ifdef TARGET_TX

#include "CRSFHandset.h"
#include "POWERMGNT.h"
#include "devHandset.h"

#include "CRSFEndpoint.h"
#include "options.h"

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
#if defined(PLATFORM_ESP8266)
    // AirPort dedicates UART0 to the transparent serial (see setupSerial in
    // tx_main.cpp). The CRSF handset would reconfigure/steal UART0, so it must
    // not start in AirPort mode.
    if (firmwareOptions.is_airport)
        return DURATION_NEVER;
#endif
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

#include "targets.h"

#ifdef TARGET_TX

#include "CRSF.h"
#include "CRSFController.h"
#include "POWERMGNT.h"
#include "devController.h"

#if defined(PLATFORM_ESP32)
#include "AutoDetect.h"
#endif

Controller *controller;

static void initialize()
{
#if defined(PLATFORM_ESP32)
    if (GPIO_PIN_RCSIGNAL_RX == GPIO_PIN_RCSIGNAL_TX)
    {
        controller = new AutoDetect();
        return;
    }
#endif
    controller = new CRSFController();
}

static int start()
{
    controller->Begin();
#if defined(DEBUG_TX_FREERUN)
    if (!controller->connect())
    {
        ERRLN("CRSF::connected has not been initialised");
    }
#endif
    return DURATION_IMMEDIATELY;
}

static int timeout()
{
    controller->handleInput();
    return DURATION_IMMEDIATELY;
}

static int event()
{
    // An event should be generated every time the TX power changes
    CRSF::LinkStatistics.uplink_TX_Power = powerToCrsfPower(PowerLevelContainer::currPower());
    return DURATION_IGNORE;
}

device_t Controller_device = {
    .initialize = initialize,
    .start = start,
    .event = event,
    .timeout = timeout};
#endif

#include "targets.h"
#include "devCRSF.h"

#ifdef CRSF_RX_MODULE
#include "common.h"

extern CRSF crsf;

static volatile bool sendFrame = false;
extern void HandleUARTin();

void ICACHE_RAM_ATTR crsfRCFrameAvailable()
{
    #if defined(PLATFORM_ESP32) || defined(PLATFORM_STM32)
    sendFrame = true;
    #else
    crsf.sendRCFrameToFC();
    #endif
}

static int start()
{
    return DURATION_IMMEDIATELY;
}

static int timeout()
{
    if (connectionState != serialUpdate)
    {
        #if defined(PLATFORM_ESP32) || defined(PLATFORM_STM32)
        if (sendFrame)
        {
            sendFrame = false;
            crsf.sendRCFrameToFC();
        }
        #endif
        crsf.RXhandleUARTout();
        HandleUARTin();
    }

    return DURATION_IMMEDIATELY;
}

device_t CRSF_device = {
    .initialize = nullptr,
    .start = start,
    .event = nullptr,
    .timeout = timeout
};
#endif

#include "targets.h"

#if defined(TARGET_RX)

#include "common.h"
#include "device.h"
#include "SerialIO.h"
#include "CRSF.h"

extern SerialIO *serialIO;

static volatile bool frameAvailable = false;

void ICACHE_RAM_ATTR crsfRCFrameAvailable()
{
    frameAvailable = true;
}

static int start()
{
    return DURATION_IMMEDIATELY;
}

static int timeout()
{
    if (connectionState != serialUpdate)
    {
        uint32_t duration = serialIO->sendRCFrameToFC(frameAvailable, ChannelData);
        frameAvailable = false;
        serialIO->handleUARTout();
        serialIO->handleUARTin();
        return duration;
    }
    return DURATION_IMMEDIATELY;
}

device_t Serial_device = {
    .initialize = nullptr,
    .start = start,
    .event = nullptr,
    .timeout = timeout
};

#endif
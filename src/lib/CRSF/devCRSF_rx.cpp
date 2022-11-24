#include "targets.h"
#include "devCRSF.h"

#ifdef CRSF_RX_MODULE
#include "common.h"

extern CRSF crsf;

static volatile bool sendFrame = false;
extern void HandleUARTin();
extern bool sbusSerialOutput;

void ICACHE_RAM_ATTR crsfRCFrameAvailable()
{
    sendFrame = true;
}

static int start()
{
    return DURATION_IMMEDIATELY;
}

static int timeout()
{
    if (connectionState != serialUpdate)
    {
        if (sendFrame)
        {
            sendFrame = false;
            crsf.sendRCFrameToFC();
        }
        if (sbusSerialOutput)
        {
            return 9;   // call us in 9ms please!
        }
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

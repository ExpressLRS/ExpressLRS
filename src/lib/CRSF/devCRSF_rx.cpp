#include "targets.h"
#include "devCRSF.h"

#ifdef CRSF_RX_MODULE
#include "common.h"

#define SBUS_FLAG_SIGNAL_LOSS       (1 << 2)
#define SBUS_FLAG_FAILSAFE_ACTIVE   (1 << 3)

extern CRSF crsf;

static volatile bool frameAvailable = false;
extern void HandleUARTin();
extern bool sbusSerialOutput;

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
        if (sbusSerialOutput)
        {
            uint8_t extraData = 0;
            if (!frameAvailable)
            {
                extraData |= SBUS_FLAG_SIGNAL_LOSS;
            }
            crsf.sendRCFrameToFC(extraData);
            frameAvailable = false;
            return 9;   // call us in 9ms please!
        }
        else
        {
            if (frameAvailable)
            {
                crsf.sendRCFrameToFC();
                frameAvailable = false;
            }
            crsf.RXhandleUARTout();
            HandleUARTin();
        }
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

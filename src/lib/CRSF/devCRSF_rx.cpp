#include "targets.h"
#include "devCRSF.h"
#include "stubborn_receiver.h"

#ifdef CRSF_RX_MODULE
extern StubbornReceiver MspReceiver;
extern uint8_t MspData[ELRS_MSP_BUFFER];
extern CRSF crsf;
extern void HandleUARTin();
extern void MspReceiveComplete();

static volatile bool sendFrame = false;

void ICACHE_RAM_ATTR crsfRCFrameAvailable()
{
    #if defined(PLATFORM_ESP32)
    sendFrame = true;
    #else
    crsf.sendRCFrameToFC();
    #endif
}

static int start()
{
    MspReceiver.SetDataToReceive(ELRS_MSP_BUFFER, MspData, ELRS_MSP_BYTES_PER_CALL);
    CRSF::Begin();
    return DURATION_IMMEDIATELY;
}

static int timeout()
{
    #if defined(PLATFORM_ESP32)
    if (sendFrame)
    {
        sendFrame = false;
        crsf.sendRCFrameToFC();
    }
    #endif

    HandleUARTin();
    crsf.RXhandleUARTout();

    if (MspReceiver.HasFinishedData())
    {
        MspReceiveComplete();
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

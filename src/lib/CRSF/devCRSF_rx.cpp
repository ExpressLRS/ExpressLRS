#include "targets.h"
#include "devCRSF.h"
#include "stubborn_receiver.h"

#ifdef CRSF_RX_MODULE
extern StubbornReceiver MspReceiver;
extern uint8_t MspData[ELRS_MSP_BUFFER];
extern CRSF crsf;
extern void HandleUARTin();
extern void MspReceiveComplete();

static bool sendFrame = false;

void crsfRCFrameAvailable()
{
    sendFrame = true;
}

static int start()
{
    MspReceiver.SetDataToReceive(ELRS_MSP_BUFFER, MspData, ELRS_MSP_BYTES_PER_CALL);
    CRSF::Begin();
    return DURATION_IMMEDIATELY;
}

static int timeout()
{
    HandleUARTin();
    crsf.RXhandleUARTout();

    if (sendFrame)
    {
        sendFrame = false;
        crsf.sendRCFrameToFC();
    }

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

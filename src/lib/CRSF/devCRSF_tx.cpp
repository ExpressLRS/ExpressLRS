#include "targets.h"
#include "devCRSF.h"

#ifdef CRSF_TX_MODULE
static int start()
{
    CRSF::Begin();
#if defined(DEBUG_TX_FREERUN)
    CRSF::CRSFstate = true;
    extern void UARTconnected();
    UARTconnected();
#endif
    return DURATION_IMMEDIATELY;
}

static int timeout()
{
    CRSF::handleUARTin();
    return DURATION_IMMEDIATELY;
}

static int event()
{
    // An event should be generated every time the TX power changes
    CRSF::LinkStatistics.uplink_TX_Power = powerToCrsfPower(PowerLevelContainer::currPower());
    return DURATION_IGNORE;
}

device_t CRSF_device = {
    .initialize = nullptr,
    .start = start,
    .event = event,
    .timeout = timeout
};
#endif

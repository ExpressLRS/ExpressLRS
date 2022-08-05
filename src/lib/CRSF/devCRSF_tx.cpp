#include "targets.h"
#include "devCRSF.h"

#ifdef CRSF_TX_MODULE

#include "../POWERMGNT/POWERMGNT.h"

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

static uint8_t powerToCrsfPower(PowerLevels_e Power)
{
    // Crossfire's power levels as defined in opentx:radio/src/telemetry/crossfire.cpp
    //static const int32_t power_values[] = { 0, 10, 25, 100, 500, 1000, 2000, 250, 50 };
    switch (Power)
    {
    case PWR_10mW: return 1;
    case PWR_25mW: return 2;
    case PWR_50mW: return 8;
    case PWR_100mW: return 3;
    case PWR_250mW: return 7;
    case PWR_500mW: return 4;
    case PWR_1000mW: return 5;
    case PWR_2000mW: return 6;
    default:
        return 0;
    }
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

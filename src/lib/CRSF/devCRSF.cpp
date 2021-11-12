#include "targets.h"
#include "common.h"
#include "device.h"

#include "CRSF.h"

static int start()
{
    CRSF::Begin();
    return DURATION_IMMEDIATELY;
}

static int timeout()
{
    CRSF::handleUARTin();
    return DURATION_IMMEDIATELY;
}

device_t CRSF_device = {
    .initialize = nullptr,
    .start = start,
    .event = nullptr,
    .timeout = timeout
};

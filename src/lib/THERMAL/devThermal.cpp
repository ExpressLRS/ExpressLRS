#include "targets.h"
#include "common.h"
#include "device.h"

#ifdef HAS_THERMAL

#include "thermal.h"
#include "POWERMGNT.h"
#include "config.h"
#include "screen.h"
#include "gsensor.h"

#if defined(TARGET_TX)
extern TxConfig config;
#else
extern RxConfig config;
#endif

Thermal thermal;

#define THERMAL_DURATION    1000

extern bool ICACHE_RAM_ATTR IsArmed();

static void initialize()
{
    thermal.init();
}

static int start()
{
    return DURATION_IMMEDIATELY;
}


static int event()
{
    thermal.update_threshold(config.GetFanMode());

    return DURATION_IGNORE;
}

static int timeout()
{
    if(!IsArmed() && connectionState != wifiUpdate)
    {
        thermal.handle();
    }
    return THERMAL_DURATION;
}

device_t Thermal_device = {
    .initialize = initialize,
    .start = start,
    .event = event,
    .timeout = timeout
};

#endif
#include "targets.h"
#include "common.h"
#include "device.h"

#ifdef HAS_GSENSOR

#include "gsensor.h"
#include "POWERMGNT.h"
#include "config.h"
#include "logging.h"

#if defined(TARGET_TX)
extern TxConfig config;
#else
extern RxConfig config;
#endif

Gsensor gsensor;

static int system_quiet_state = GSENSOR_SYSTEM_STATE_MOVING;
static int system_quiet_pre_state = GSENSOR_SYSTEM_STATE_MOVING;
static int bumps = 0;
static unsigned long firstBumpTime = 0;
static unsigned long lastBumpTime = 0;

extern bool ICACHE_RAM_ATTR IsArmed();

#define GSENSOR_DURATION    10

static void initialize()
{
    gsensor.init();
}

static int start()
{
    return DURATION_IMMEDIATELY;
}

static int timeout()
{
    static unsigned long lastCall = 0;
    unsigned long now = millis();
    if (gsensor.handleBump(now))
    {
        if (bumps == 0)
        {
            firstBumpTime = now;
        }
        lastBumpTime = now;
        bumps++;
    }
    if (bumps > 0 && (now - lastBumpTime > 400 || now - firstBumpTime > 1000))
    {
        DBGLN("Bumps %d", bumps);
        bumps = 0;
    }
    if (now - lastCall > 1000) {
        gsensor.handle();

        system_quiet_state = gsensor.getSystemState();
        //When system is idle, set power to minimum
        if(config.GetMotionMode() == 1)
        {
            if((system_quiet_state == GSENSOR_SYSTEM_STATE_QUIET) && (system_quiet_pre_state == GSENSOR_SYSTEM_STATE_MOVING) && !IsArmed())
            {
                POWERMGNT::setPower(MinPower);
            }
            if((system_quiet_state == GSENSOR_SYSTEM_STATE_MOVING) && (system_quiet_pre_state == GSENSOR_SYSTEM_STATE_QUIET))
            {
                POWERMGNT::setPower((PowerLevels_e)config.GetPower());
            }
        }
        system_quiet_pre_state = system_quiet_state;
        lastCall = now;
    }
    return 10;
}

device_t Gsensor_device = {
    .initialize = initialize,
    .start = start,
    .event = NULL,
    .timeout = timeout
};

#endif
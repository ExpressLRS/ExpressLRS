#include "targets.h"
#include "common.h"
#include "device.h"

#ifdef HAS_GSENSOR

#include <functional>

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
static unsigned int bumps = 0;
static unsigned long lastBumpTime = 0;

extern bool IsArmed();
extern void SendRxLoanOverMSP();
extern void EnterBindingMode();
extern void deferExecution(uint32_t ms, std::function<void()> f);

#define GSENSOR_DURATION    10
#define GSENSOR_SYSTEM_IDLE_INTERVAL 1000U

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
    static unsigned long lastIdleCheckMs = 0;
    unsigned long now = millis();
    if (gsensor.handleBump(now))
    {
        lastBumpTime = now;
        bumps++;
    }
    if (bumps > 0 && (now - lastBumpTime > 40))
    {
        float x, y, z;
        gsensor.getGSensorData(&x, &y, &z);
        if (bumps == 1 && fabs(x) < 0.5 && y < -0.8 && fabs(z) < 0.5)   // Single bump while holding the radio antenna up
        {
            if (connectionState == connected)
            {
                DBGLN("Loaning model");
                SendRxLoanOverMSP();
            }
            else
            {
                DBGLN("Borrowing model");
                // defer this calling `EnterBindingMode` for 2 seconds
                deferExecution(2000, EnterBindingMode);
            }
        }
        DBGLN("Bumps %d : %f %f %f", bumps, x, y, z);
        bumps = 0;
    }
    if (now - lastIdleCheckMs > GSENSOR_SYSTEM_IDLE_INTERVAL)
    {
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
        lastIdleCheckMs = now;
    }
    return GSENSOR_DURATION;
}

device_t Gsensor_device = {
    .initialize = initialize,
    .start = start,
    .event = NULL,
    .timeout = timeout
};

#endif

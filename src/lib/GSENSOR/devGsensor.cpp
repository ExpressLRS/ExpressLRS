#include "targets.h"
#include "common.h"

#ifdef HAS_GSENSOR
#if !defined(OPT_HAS_GSENSOR)
#define OPT_HAS_GSENSOR true
#endif

#include "devGsensor.h"
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

#define GSENSOR_DURATION    10
#define GSENSOR_SYSTEM_IDLE_INTERVAL 1000U

#define MULTIPLE_BUMP_INTERVAL 400U
#define BUMP_COMMAND_IDLE_TIME 10000U

static bool gSensorOk = false;

static void initialize()
{
    if (OPT_HAS_GSENSOR && GPIO_PIN_SCL != UNDEF_PIN && GPIO_PIN_SDA != UNDEF_PIN)
    {
        gSensorOk = gsensor.init();
    }
}

static int start()
{
    if (gSensorOk && OPT_HAS_GSENSOR && GPIO_PIN_SCL != UNDEF_PIN && GPIO_PIN_SDA != UNDEF_PIN)
    {
        return DURATION_IMMEDIATELY;
    }
    return DURATION_NEVER;
}

static int timeout()
{
    static unsigned long lastIdleCheckMs = 0;
    unsigned long now = millis();
    if (now - lastIdleCheckMs > GSENSOR_SYSTEM_IDLE_INTERVAL)
    {
        gsensor.handle();

        system_quiet_state = gsensor.getSystemState();
        //When system is idle, set power to minimum
        if(config.GetMotionMode() == 1)
        {
            if((system_quiet_state == GSENSOR_SYSTEM_STATE_QUIET) && (system_quiet_pre_state == GSENSOR_SYSTEM_STATE_MOVING) && !CRSF::IsArmed())
            {
                POWERMGNT::setPower(MinPower);
            }
            if((system_quiet_state == GSENSOR_SYSTEM_STATE_MOVING) && (system_quiet_pre_state == GSENSOR_SYSTEM_STATE_QUIET) && POWERMGNT::currPower() < (PowerLevels_e)config.GetPower())
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

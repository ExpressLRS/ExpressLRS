#include "targets.h"
#include "common.h"

#ifdef HAS_GSENSOR
#if !defined(OPT_HAS_GSENSOR)
#define OPT_HAS_GSENSOR true
#endif

#include "devGsensor.h"
#include "gsensor.h"
#include "POWERMGNT.h"
#include "config.h"

#if defined(TARGET_TX)
extern TxConfig config;
#else
extern RxConfig config;
#endif

Gsensor gsensor;

int system_quiet_state = GSENSOR_SYSTEM_STATE_MOVING;
int system_quiet_pre_state = GSENSOR_SYSTEM_STATE_MOVING;

#define GSENSOR_DURATION    1000

static void initialize()
{
    if (OPT_HAS_GSENSOR && GPIO_PIN_SCL != UNDEF_PIN && GPIO_PIN_SDA != UNDEF_PIN)
    {
        gsensor.init();
    }
}

static int start()
{
    if (OPT_HAS_GSENSOR && GPIO_PIN_SCL != UNDEF_PIN && GPIO_PIN_SDA != UNDEF_PIN)
    {
        return DURATION_IMMEDIATELY;
    }
    return DURATION_NEVER;
}

static int timeout()
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
        if((system_quiet_state == GSENSOR_SYSTEM_STATE_MOVING) && (system_quiet_pre_state == GSENSOR_SYSTEM_STATE_QUIET))
        {
            POWERMGNT::setPower((PowerLevels_e)config.GetPower());
        }
    }
    system_quiet_pre_state = system_quiet_state;

    return GSENSOR_DURATION;
}

device_t Gsensor_device = {
    .initialize = initialize,
    .start = start,
    .event = NULL,
    .timeout = timeout
};

#endif
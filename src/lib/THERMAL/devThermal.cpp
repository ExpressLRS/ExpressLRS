#include "targets.h"
#include "devThermal.h"

#if defined(HAS_THERMAL) || defined(HAS_FAN)

#if defined(TARGET_RX)
    #error "devThermal not supported on RX"
#endif

#include "config.h"
extern TxConfig config;

#define THERMAL_DURATION 1000

#if defined(HAS_THERMAL)
#include "common.h"
#include "thermal.h"

Thermal thermal;

#if defined(HAS_SMART_FAN)
bool is_smart_fan_control = false;
bool is_smart_fan_working = false;
#endif

#ifdef HAS_THERMAL_LM75A
    #ifndef OPT_HAS_THERMAL_LM75A
        #define OPT_HAS_THERMAL_LM75A true
    #endif
#else
    #define OPT_HAS_THERMAL_LM75A true
#endif

#endif // HAS_THERMAL

#include "POWERMGNT.h"

static void initialize()
{
    #if defined(HAS_THERMAL)
    if (OPT_HAS_THERMAL_LM75A && GPIO_PIN_SCL != UNDEF_PIN && GPIO_PIN_SDA != UNDEF_PIN)
    {
        thermal.init();
    }
    #endif
    if (GPIO_PIN_FAN_EN != UNDEF_PIN)
    {
        pinMode(GPIO_PIN_FAN_EN, OUTPUT);
    }
}

#if defined(HAS_THERMAL)
static void timeoutThermal()
{
    if(OPT_HAS_THERMAL_LM75A && !CRSF::IsArmed() && connectionState != wifiUpdate)
    {
        thermal.handle();
 #ifdef HAS_SMART_FAN
        if(is_smart_fan_control & !is_smart_fan_working){
            is_smart_fan_working = true;
            thermal.update_threshold(USER_SMARTFAN_OFF);
        }
        if(!is_smart_fan_control & is_smart_fan_working){
            is_smart_fan_working = false;
#endif
            thermal.update_threshold(config.GetFanMode());
#ifdef HAS_SMART_FAN
        }
#endif
    }
}
#endif

/***
 * Checks the PowerFanThreshold vs CurrPower and enables the fan if at or above the threshold
 * using a hysteresis. To turn on it must be at/above the threshold for a small time
 * and then to turn off it must be below the threshold for FAN_MIN_RUNTIME intervals
 ***/
static void timeoutFan()
{
    static uint8_t fanStateDuration;
    static bool fanIsOn;
    bool fanShouldBeOn = POWERMGNT::currPower() >= (PowerLevels_e)config.GetPowerFanThreshold();

    if (fanIsOn)
    {
        if (fanShouldBeOn)
        {
            fanStateDuration = 0; // reset the timeout
        }
        else if (fanStateDuration < firmwareOptions.fan_min_runtime)
        {
            ++fanStateDuration; // counting to turn off
        }
        else
        {
            // turn off expired
            digitalWrite(GPIO_PIN_FAN_EN, LOW);
            fanStateDuration = 0;
            fanIsOn = false;
        }
    }
    // vv else fan is off currently vv
    else if (fanShouldBeOn)
    {
        // Delay turning the fan on for 4 cycles to be sure it really should be on
        if (fanStateDuration < 3)
        {
            ++fanStateDuration; // counting to turn on
        }
        else
        {
            digitalWrite(GPIO_PIN_FAN_EN, HIGH);
            fanStateDuration = 0;
            fanIsOn = true;
        }
    }
}

static int event()
{
#if defined(HAS_THERMAL)
    if (OPT_HAS_THERMAL_LM75A && GPIO_PIN_SCL != UNDEF_PIN && GPIO_PIN_SDA != UNDEF_PIN)
    {
#ifdef HAS_SMART_FAN
        if(!is_smart_fan_control)
        {
#endif
            thermal.update_threshold(config.GetFanMode());
#ifdef HAS_SMART_FAN
        }
#endif
    }
#endif
    return THERMAL_DURATION;
}

static int timeout()
{
#if defined(HAS_THERMAL)
    if (OPT_HAS_THERMAL_LM75A && GPIO_PIN_SCL != UNDEF_PIN && GPIO_PIN_SDA != UNDEF_PIN)
    {
        timeoutThermal();
    }
#endif
    if (GPIO_PIN_FAN_EN != UNDEF_PIN)
    {
        timeoutFan();
    }

    return THERMAL_DURATION;
}

device_t Thermal_device = {
    .initialize = initialize,
    .start = nullptr,
    .event = event,
    .timeout = timeout
};

#endif // HAS_THERMAL || HAS_FAN
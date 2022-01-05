#include "devThermal.h"

#if defined(HAS_THERMAL) || defined(HAS_FAN)

#if defined(TARGET_RX)
    #error "devThermal not supported on RX"
#endif

#include "config.h"
extern TxConfig config;

#define THERMAL_DURATION 1000

#if defined(HAS_THERMAL)
//#include "targets.h"
//#include "common.h"
extern bool IsArmed();

Thermal thermal;

#if defined(HAS_SMART_FAN)
bool is_smart_fan_control = false;
bool is_smart_fan_working = false;
#endif

#endif // HAS_THERMAL

#if defined(HAS_FAN)
#include "POWERMGNT.h"

#if !defined(FAN_MIN_RUNTIME)
    #define FAN_MIN_RUNTIME 60U // seconds
#endif

#endif

static void initialize()
{
#if defined(HAS_THERMAL)
    thermal.init();
#endif
#if defined(HAS_FAN)
    pinMode(GPIO_PIN_FAN_EN, OUTPUT);
#endif
}

static void timeoutThermal()
{
#if defined(HAS_THERMAL)
    if(!IsArmed() && connectionState != wifiUpdate)
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
#endif // HAS_THERMAL
}

static void timeoutFan()
{
#if defined(HAS_FAN)
    static uint8_t fanOnDuration;

    // Enable the fan if at or above the threshold
    if (POWERMGNT::currPower() >= (PowerLevels_e)config.GetPowerFanThreshold())
    {
        digitalWrite(GPIO_PIN_FAN_EN, HIGH);
        fanOnDuration = 0;
    }
    // Disable the fan if it has been below the threshold for long enough
    else
    {
        if (fanOnDuration < FAN_MIN_RUNTIME)
            ++fanOnDuration;
        else
            digitalWrite(GPIO_PIN_FAN_EN, LOW);
    }
#endif // HAS_FAN
}

static int event()
{
#if defined(HAS_THERMAL)
#ifdef HAS_SMART_FAN
    if(!is_smart_fan_control)
    {
#endif
        thermal.update_threshold(config.GetFanMode());
#ifdef HAS_SMART_FAN
    }
#endif
#endif // HAS_THERMAL

    return THERMAL_DURATION;
}

static int timeout()
{
    timeoutThermal();
    timeoutFan();

    return THERMAL_DURATION;
}

device_t Thermal_device = {
    .initialize = initialize,
    .start = NULL,
    .event = event,
    .timeout = timeout
};

#endif // HAS_THERMAL || HAS_FAN
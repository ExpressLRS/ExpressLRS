#include "targets.h"
#include "devThermal.h"

#if defined(HAS_THERMAL) || defined(HAS_FAN)

#include "config.h"
#include "logging.h"

#if defined(TARGET_RX)
extern RxConfig config;
#else
extern TxConfig config;
#endif
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

#if defined(GPIO_PIN_FAN_PWM)
constexpr uint8_t fanChannel = 0;
#endif

#if !defined(FAN_MIN_RUNTIME)
    #define FAN_MIN_RUNTIME 30U // intervals (seconds)
#endif

#define FAN_MIN_CHANGETIME 10U  // intervals (seconds)

#if !defined(TACHO_PULSES_PER_REV)
#define TACHO_PULSES_PER_REV 4
#endif

static uint16_t currentRPM = 0;

void init_rpm_counter(int pin);
uint32_t get_rpm();

static void initialize()
{
#if defined(HAS_THERMAL)
#if defined(PLATFORM_ESP32_S3)
    thermal.init();
#else
    if (OPT_HAS_THERMAL_LM75A && GPIO_PIN_SCL != UNDEF_PIN && GPIO_PIN_SDA != UNDEF_PIN)
    {
        thermal.init();
    }
#endif
#endif
    if (GPIO_PIN_FAN_EN != UNDEF_PIN)
    {
        pinMode(GPIO_PIN_FAN_EN, OUTPUT);
    }
}

static void timeoutThermal()
{
#if defined(HAS_THERMAL)
#if !defined(PLATFORM_ESP32_S3)
    if(OPT_HAS_THERMAL_LM75A)
#endif
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
#endif
}

#if defined(PLATFORM_ESP32)
#ifndef TARGET_RX
static void setFanSpeed()
{
    const uint8_t defaultFanSpeeds[] = {
        31,  // 10mW
        47,  // 25mW
        63,  // 50mW
        95,  // 100mW
        127, // 250mW
        191, // 500mW
        255, // 1000mW
        255  // 2000mW
    };

    uint32_t speed = GPIO_PIN_FAN_SPEEDS == nullptr ? defaultFanSpeeds[POWERMGNT::currPower()] : GPIO_PIN_FAN_SPEEDS[POWERMGNT::currPower()-POWERMGNT::getMinPower()];
    ledcWrite(fanChannel, speed);
    //DBGLN("Fan speed: %d (power) -> %u (pwm)", POWERMGNT::currPower(), speed);
}
#endif
#endif

/*
 * For enable-only fans:
 *  Checks the PowerFanThreshold vs CurrPower and enables the fan if at or above the threshold
 *  using a hysteresis. To turn on it must be at/above the threshold for a small time
 *  and then to turn off it must be below the threshold for FAN_MIN_RUNTIME intervals.
 * For PWM fans:
 *  all of the above applies, but rather than just turning the fan on, the speed of the fan
 *  is set according to the power output level.
 */
static void timeoutFan()
{
#if defined(HAS_FAN)
    static uint8_t fanStateDuration;
    static bool fanIsOn;
#if defined(TARGET_RX)
    bool fanShouldBeOn = true;
#else
    bool fanShouldBeOn = POWERMGNT::currPower() >= (PowerLevels_e)config.GetPowerFanThreshold();
#endif
    if (fanIsOn)
    {
        if (fanShouldBeOn)
        {
#if defined(PLATFORM_ESP32)
#ifndef TARGET_RX
            if (GPIO_PIN_FAN_PWM != UNDEF_PIN)
            {
                static PowerLevels_e lastPower = MinPower;
                if (POWERMGNT::currPower() < lastPower && fanStateDuration < FAN_MIN_CHANGETIME)
                {
                    ++fanStateDuration;
                }
                if (POWERMGNT::currPower() > lastPower || fanStateDuration >= FAN_MIN_CHANGETIME)
                {
                    setFanSpeed();
                    lastPower = POWERMGNT::currPower();
                    fanStateDuration = 0; // reset the timeout
                }
            }
            else
#endif
#endif
            {
                fanStateDuration = 0; // reset the timeout
            }
        }
        else if (fanStateDuration < firmwareOptions.fan_min_runtime)
        {
            ++fanStateDuration; // counting to turn off
        }
        else
        {
            // turn off expired
            if (GPIO_PIN_FAN_EN != UNDEF_PIN)
            {
                digitalWrite(GPIO_PIN_FAN_EN, LOW);
            }
#if defined(PLATFORM_ESP32)
            else if (GPIO_PIN_FAN_PWM != UNDEF_PIN)
            {
                ledcWrite(fanChannel, 0);
            }
#endif
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
            if (GPIO_PIN_FAN_EN != UNDEF_PIN)
            {
                digitalWrite(GPIO_PIN_FAN_EN, HIGH);
                fanStateDuration = 0;
            }
#if defined(PLATFORM_ESP32)
            else if (GPIO_PIN_FAN_PWM != UNDEF_PIN)
            {
                // bump the fan to full power for one cycle in case
                // the PWM level is not sufficient to get it moving
                ledcWrite(fanChannel, 192);
                fanStateDuration = FAN_MIN_CHANGETIME;
            }
#endif
            fanIsOn = true;
        }
    }
#endif
}

uint16_t getCurrentRPM()
{
    return currentRPM;
}

static void timeoutTacho()
{
#if defined(PLATFORM_ESP32)
    if (GPIO_PIN_FAN_TACHO != UNDEF_PIN)
    {
        currentRPM = get_rpm();
        DBGVLN("RPM %d", currentRPM);
    }
#endif
}

static int start()
{
#if defined(PLATFORM_ESP32)
    if (GPIO_PIN_FAN_PWM != UNDEF_PIN)
    {
        ledcSetup(fanChannel, 25000, 8);
        ledcAttachPin(GPIO_PIN_FAN_PWM, fanChannel);
        ledcWrite(fanChannel, 0);
    }
    if (GPIO_PIN_FAN_TACHO != UNDEF_PIN)
    {
        init_rpm_counter(GPIO_PIN_FAN_TACHO);
    }
#endif
    return DURATION_IMMEDIATELY;
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
    return DURATION_IGNORE;
}

static int timeout()
{
    timeoutThermal();
    timeoutFan();
    timeoutTacho();
    return THERMAL_DURATION;
}

device_t Thermal_device = {
    .initialize = initialize,
    .start = start,
    .event = event,
    .timeout = timeout
};

#endif // HAS_THERMAL || HAS_FAN

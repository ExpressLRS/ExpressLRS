#include "targets.h"
#include "common.h"
#include "config.h"

#include <functional>

#if defined(USE_I2C)
#include <Wire.h>
#endif

static uint32_t startDeferredTime = 0;
static uint32_t deferredTimeout = 0;
static std::function<void()> deferredFunction = nullptr;

static void setupWire()
{
#if defined(USE_I2C)
    int scl = GPIO_PIN_SCL;
    int sda = GPIO_PIN_SDA;
#if defined(TARGET_RX) && defined(GPIO_PIN_PWM_OUTPUTS)
    // If I2C pins are not defined in the hardware, then look for configured I2C
    if (GPIO_PIN_SCL == UNDEF_PIN || GPIO_PIN_SDA == UNDEF_PIN)
    {
        // find the configured SCL/SDA pins, if any
        for (unsigned ch = 0 ; ch < GPIO_PIN_PWM_OUTPUTS_COUNT ; ++ch)
        {
            auto pin = GPIO_PIN_PWM_OUTPUTS[ch];
            auto pwm = config.GetPwmChannel(ch);
            if (pin == GPIO_PIN_SCL && pwm->val.mode != somSCL)
            {
                scl = pin;
            }
            if (pin == GPIO_PIN_SDA && pwm->val.mode != somSDA)
            {
                sda = pin;
            }
        }
    }
    else
    {
        for (unsigned ch = 0 ; ch < GPIO_PIN_PWM_OUTPUTS_COUNT ; ++ch)
        {
            auto pin = GPIO_PIN_PWM_OUTPUTS[ch];
            auto pwm = config.GetPwmChannel(ch);
            // if the pin is SDA or SCL and it's not configured for I2C then undef the pins
            if ((pin == GPIO_PIN_SCL && pwm->val.mode != somSCL) || (pin == GPIO_PIN_SDA && pwm->val.mode != somSDA))
            {
                scl = UNDEF_PIN;
                sda = UNDEF_PIN;
                break;
            }
        }
    }
#endif
    if(sda != UNDEF_PIN && scl != UNDEF_PIN)
    {
#if defined(PLATFORM_STM32)
        // Wire::begin() passing ints is ambiguously overloaded, use the set functions
        // which themselves might get the PinName overloads
        Wire.setSCL(scl);
        Wire.setSDA(sda);
        Wire.begin();
#else
        // ESP hopes to get Wire::begin(int, int)
        // ESP32 hopes to get Wire::begin(int = -1, int = -1, uint32 = 0)
        Wire.begin(sda, scl);
        Wire.setClock(400000);
#endif
    }
#endif
}

void setupTargetCommon()
{
    setupWire();
}

void deferExecution(uint32_t ms, std::function<void()> f)
{
    startDeferredTime = millis();
    deferredTimeout = ms;
    deferredFunction = f;
}

void executeDeferredFunction(unsigned long now)
{
    // execute deferred function if its time has elapsed
    if (deferredFunction != nullptr && (now - startDeferredTime) > deferredTimeout)
    {
        deferredFunction();
        deferredFunction = nullptr;
    }
}

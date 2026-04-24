#include "targets.h"
#include "common.h"

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
    if(GPIO_PIN_SDA != UNDEF_PIN && GPIO_PIN_SCL != UNDEF_PIN)
    {
#if defined(PLATFORM_STM32)
        // Wire::begin() passing ints is ambiguously overloaded, use the set functions
        // which themselves might get the PinName overloads
        Wire.setSCL(GPIO_PIN_SCL);
        Wire.setSDA(GPIO_PIN_SDA);
        Wire.begin();
#else
        // ESP hopes to get Wire::begin(int, int)
        // ESP32 hopes to get Wire::begin(int = -1, int = -1, uint32 = 0)
        Wire.begin((int)GPIO_PIN_SDA, (int)GPIO_PIN_SCL);
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

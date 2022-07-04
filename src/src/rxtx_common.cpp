#include "targets.h"

#include <functional>

#if defined(USE_I2C)
#include <Wire.h>
#endif

static uint32_t startDeferredTime = 0;
static uint32_t deferredTimeout = 0;
static std::function<void()> deferredFunction = nullptr;

void setupTargetCommon()
{
#if defined(USE_I2C)
  if(GPIO_PIN_SDA != UNDEF_PIN && GPIO_PIN_SCL != UNDEF_PIN)
  {
#if defined(PLATFORM_STM32)
    Wire.setSCL(GPIO_PIN_SCL);
    Wire.setSDA(GPIO_PIN_SDA);
#else
    Wire.setPins(GPIO_PIN_SDA, GPIO_PIN_SDA);
#endif
    Wire.begin();
  }
#endif
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

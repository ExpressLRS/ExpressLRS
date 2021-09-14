#ifdef PLATFORM_ESP8266
#include "ESP8266_hwTimer.h"

void inline hwTimer::nullCallback(void) {}

void (*hwTimer::callbackTick)() = &nullCallback;
void (*hwTimer::callbackTock)() = &nullCallback;

volatile uint32_t hwTimer::HWtimerInterval = TimerIntervalUSDefault;
volatile bool hwTimer::isTick = false;
volatile int32_t hwTimer::PhaseShift = 0;
volatile int32_t hwTimer::FreqOffset = 0;
bool hwTimer::running = false;

#define HWTIMER_TICKS_PER_US 5

void hwTimer::init()
{
    if (!running)
    {
        timer1_attachInterrupt(hwTimer::callback);
        timer1_enable(TIM_DIV16, TIM_EDGE, TIM_LOOP); //5MHz ticks
        timer1_write(hwTimer::HWtimerInterval >> 1);  //120000 us
        isTick = false;
        running = true;
    }
}

void hwTimer::stop()
{
    if (running)
    {
        timer1_disable();
        timer1_detachInterrupt();
        running = false;
    }
}

void ICACHE_RAM_ATTR hwTimer::resume()
{
    init();
    // The STM32 timer fires tock() ASAP after enabling, so mimic that behavior
    // tock() should always be the first event to maintain consistency
    isTick = false;
    timer1_write(10);
}

void hwTimer::updateInterval(uint32_t newTimerInterval)
{
    // timer should not be running when updateInterval() is called
    hwTimer::HWtimerInterval = newTimerInterval * HWTIMER_TICKS_PER_US;
}

void ICACHE_RAM_ATTR hwTimer::resetFreqOffset()
{
    FreqOffset = 0;
}

void ICACHE_RAM_ATTR hwTimer::incFreqOffset()
{
    FreqOffset++;
}

void ICACHE_RAM_ATTR hwTimer::decFreqOffset()
{
    FreqOffset--;
}

void ICACHE_RAM_ATTR hwTimer::phaseShift(int32_t newPhaseShift)
{
    int32_t minVal = -(hwTimer::HWtimerInterval >> 2);
    int32_t maxVal = (hwTimer::HWtimerInterval >> 2);

    hwTimer::PhaseShift = constrain(newPhaseShift, minVal, maxVal) * HWTIMER_TICKS_PER_US;
}

void ICACHE_RAM_ATTR hwTimer::callback()
{
    if (!running)
    {
        return;
    }

    if (hwTimer::isTick)
    {
        timer1_write((hwTimer::HWtimerInterval >> 1) + FreqOffset);
        hwTimer::callbackTick();
    }
    else
    {
        timer1_write((hwTimer::HWtimerInterval >> 1) + hwTimer::PhaseShift + FreqOffset);
        hwTimer::PhaseShift = 0;
        hwTimer::callbackTock();
    }
    hwTimer::isTick = !hwTimer::isTick;
}
#endif

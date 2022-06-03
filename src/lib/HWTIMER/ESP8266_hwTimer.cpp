#ifdef PLATFORM_ESP8266
#include "ESP8266_hwTimer.h"

void (*hwTimer::callbackTick)() = nullptr;
void (*hwTimer::callbackTock)() = nullptr;

volatile uint32_t hwTimer::HWtimerInterval = TimerIntervalUSDefault;
volatile bool hwTimer::isTick = false;
volatile int32_t hwTimer::PhaseShift = 0;
volatile int32_t hwTimer::FreqOffset = 0;
bool hwTimer::running = false;
uint32_t hwTimer::NextTimeout;

#define HWTIMER_TICKS_PER_US 5
#define HWTIMER_PRESCALER (clockCyclesPerMicrosecond() / HWTIMER_TICKS_PER_US)


void hwTimer::init()
{
    timer0_isr_init();
    running = false;
}

void hwTimer::stop()
{
    if (running)
    {
        timer0_detachInterrupt();
        running = false;
    }
}

void ICACHE_RAM_ATTR hwTimer::resume()
{
    if (!running)
    {
        noInterrupts();
        timer0_attachInterrupt(hwTimer::callback);
        // The STM32 timer fires tock() ASAP after enabling, so mimic that behavior
        // tock() should always be the first event to maintain consistency
        isTick = false;
        // Fire the timer in 2us to get it started close to now
        NextTimeout = ESP.getCycleCount() + (2 * HWTIMER_TICKS_PER_US * HWTIMER_PRESCALER);
        timer0_write(NextTimeout);
        interrupts();

        running = true;
    }
}

void hwTimer::updateInterval(uint32_t newTimerInterval)
{
    // timer should not be running when updateInterval() is called
    hwTimer::HWtimerInterval = newTimerInterval * (HWTIMER_TICKS_PER_US * HWTIMER_PRESCALER);
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

    // phase shift is in microseconds
    hwTimer::PhaseShift = constrain(newPhaseShift, minVal, maxVal) * (HWTIMER_TICKS_PER_US * HWTIMER_PRESCALER);
}

void ICACHE_RAM_ATTR hwTimer::callback()
{
    if (!running)
    {
        return;
    }

    NextTimeout += (hwTimer::HWtimerInterval >> 1) + (FreqOffset * HWTIMER_PRESCALER);
    if (hwTimer::isTick)
    {
        timer0_write(NextTimeout);
        if (callbackTick) callbackTick();
    }
    else
    {
        NextTimeout += hwTimer::PhaseShift;
        timer0_write(NextTimeout);
        hwTimer::PhaseShift = 0;
        if (callbackTock) callbackTock();
    }
    hwTimer::isTick = !hwTimer::isTick;
}
#endif

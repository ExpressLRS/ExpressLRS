#if defined(PLATFORM_ESP8266)
#include "hwTimer.h"

void (*hwTimer::callbackTick)() = nullptr;
void (*hwTimer::callbackTock)() = nullptr;

volatile bool hwTimer::running = false;
volatile bool hwTimer::isTick = false;

volatile uint32_t hwTimer::HWtimerInterval = TimerIntervalUSDefault;
volatile int32_t hwTimer::PhaseShift = 0;
volatile int32_t hwTimer::FreqOffset = 0;

// Internal implementation specific variables
static uint32_t NextTimeout;

#define HWTIMER_TICKS_PER_US 5
#define HWTIMER_PRESCALER (clockCyclesPerMicrosecond() / HWTIMER_TICKS_PER_US)

void hwTimer::init(void (*callbackTick)(), void (*callbackTock)())
{
    hwTimer::callbackTick = callbackTick;
    hwTimer::callbackTock = callbackTock;
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
#if defined(TARGET_TX)
        NextTimeout = HWtimerInterval;
#else
        // Fire the timer in 2us to get it started close to now
        NextTimeout = ESP.getCycleCount() + (2 * HWTIMER_TICKS_PER_US * HWTIMER_PRESCALER);
#endif
        timer0_write(NextTimeout);
        interrupts();

        running = true;
    }
}

void hwTimer::updateInterval(uint32_t newTimerInterval)
{
    // timer should not be running when updateInterval() is called
    HWtimerInterval = newTimerInterval * (HWTIMER_TICKS_PER_US * HWTIMER_PRESCALER);
}

void ICACHE_RAM_ATTR hwTimer::phaseShift(int32_t newPhaseShift)
{
    int32_t minVal = -(HWtimerInterval >> 2);
    int32_t maxVal = (HWtimerInterval >> 2);

    // phase shift is in microseconds
    PhaseShift = constrain(newPhaseShift, minVal, maxVal) * (HWTIMER_TICKS_PER_US * HWTIMER_PRESCALER);
}

void ICACHE_RAM_ATTR hwTimer::callback()
{
    if (running)
    {
#if defined(TARGET_TX)
        NextTimeout += HWtimerInterval;
        timer0_write(NextTimeout);
        callbackTock();
#else
        NextTimeout += (HWtimerInterval >> 1) + (FreqOffset * HWTIMER_PRESCALER);
        if (hwTimer::isTick)
        {
            timer0_write(NextTimeout);
            callbackTick();
        }
        else
        {
            NextTimeout += PhaseShift;
            timer0_write(NextTimeout);
            PhaseShift = 0;
            callbackTock();
        }
        hwTimer::isTick = !hwTimer::isTick;
#endif
    }
}

#endif

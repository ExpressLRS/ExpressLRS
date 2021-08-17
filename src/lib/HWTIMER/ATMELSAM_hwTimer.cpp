#ifdef PLATFORM_ATMELSAM
#include "ATMELSAM_hwTimer.h"

#include "TimerInterrupt_Generic.h"

void inline hwTimer::nullCallback(void) {}

void (*hwTimer::callbackTick)() = &nullCallback;
void (*hwTimer::callbackTock)() = &nullCallback;

volatile uint32_t hwTimer::HWtimerInterval = TimerIntervalUSDefault;
volatile bool hwTimer::isTick = true;
volatile int32_t hwTimer::PhaseShift = 0;
volatile int32_t hwTimer::FreqOffset = 0;
bool hwTimer::running = false;

static SAMDTimer ITimer0(TIMER_TC3);

void hwTimer::init()
{
    if (!running)
    {
        ITimer0.attachInterruptInterval(HWtimerInterval >> 1, callback);
        isTick = true;
        running = true;
    }
}

void hwTimer::stop()
{
    if (running)
    {
        ITimer0.disableTimer();
        ITimer0.detachInterrupt();
        running = false;
    }
}

void ICACHE_RAM_ATTR hwTimer::resume()
{
    if (!running)
    {
        init();
        running = true;
    }
}

void hwTimer::updateInterval(uint32_t newTimerInterval)
{
    HWtimerInterval = newTimerInterval;
    if (running)
    {
        ITimer0.disableTimer();
        ITimer0.detachInterrupt();
        ITimer0.attachInterruptInterval(HWtimerInterval >> 1, callback);
    }
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
    int32_t minVal = -(HWtimerInterval >> 2);
    int32_t maxVal = (HWtimerInterval >> 2);

    PhaseShift = constrain(newPhaseShift, minVal, maxVal);
}

void ICACHE_RAM_ATTR hwTimer::callback()
{
    if (!running)
    {
        return;
    }

    if (isTick)
    {
        ITimer0.disableTimer();
        ITimer0.detachInterrupt();
        ITimer0.attachInterruptInterval((HWtimerInterval >> 1) + FreqOffset, callback);
        callbackTick();
    }
    else
    {
        ITimer0.disableTimer();
        ITimer0.detachInterrupt();
        ITimer0.attachInterruptInterval((HWtimerInterval >> 1) + PhaseShift + FreqOffset, callback);
        PhaseShift = 0;
        callbackTock();
    }
    isTick = !isTick;
}
#endif

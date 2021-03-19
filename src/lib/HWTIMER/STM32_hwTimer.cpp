#ifdef PLATFORM_STM32
#include "STM32_hwTimer.h"

void inline hwTimer::nullCallback(void) {}

void (*hwTimer::callbackTick)() = &nullCallback;
void (*hwTimer::callbackTock)() = &nullCallback;

volatile uint32_t hwTimer::HWtimerInterval = TimerIntervalUSDefault;
volatile bool hwTimer::TickTock = false;
volatile int32_t hwTimer::PhaseShift = 0;
volatile int32_t hwTimer::FreqOffset = 0;
bool hwTimer::running = false;
bool hwTimer::alreadyInit = false;

HardwareTimer(*hwTimer::MyTim) = new HardwareTimer(TIM1);

void hwTimer::init()
{
    if (!alreadyInit)
    {
        MyTim->attachInterrupt(hwTimer::callback);
        MyTim->setMode(1, TIMER_OUTPUT_COMPARE);
        MyTim->setOverflow(hwTimer::HWtimerInterval >> 1, MICROSEC_FORMAT);
        MyTim->setPreloadEnable(false);
        alreadyInit = true;
    }
}

void hwTimer::stop()
{
    running = false;
    MyTim->pause();
    MyTim->setCount(0);
}

void hwTimer::resume()
{
    TickTock = false;
    running = true;
    MyTim->resume();
    MyTim->refresh();
}

void hwTimer::updateInterval(uint32_t newTimerInterval)
{
    MyTim->setOverflow(hwTimer::HWtimerInterval >> 1, MICROSEC_FORMAT);
    hwTimer::HWtimerInterval = newTimerInterval;
}

void ICACHE_RAM_ATTR hwTimer::incFreqOffset()
{
    FreqOffset++;
}

void ICACHE_RAM_ATTR hwTimer::decFreqOffset()
{
    FreqOffset--;
}

void hwTimer::phaseShift(int32_t newPhaseShift)
{
    int32_t minVal = -(hwTimer::HWtimerInterval >> 4);
    int32_t maxVal = (hwTimer::HWtimerInterval >> 4);

    hwTimer::PhaseShift = constrain(newPhaseShift, minVal, maxVal);
}

void hwTimer::callback(void)
{
    if (!running)
    {
        return;
    }

    if (hwTimer::TickTock)
    {
        MyTim->setOverflow((hwTimer::HWtimerInterval >> 1), MICROSEC_FORMAT);
        uint32_t adjustedInterval = MyTim->getOverflow(TICK_FORMAT) + FreqOffset;
        MyTim->setOverflow(adjustedInterval, TICK_FORMAT);
        hwTimer::callbackTick();
    }
    else
    {
        MyTim->setOverflow((hwTimer::HWtimerInterval >> 1) + hwTimer::PhaseShift, MICROSEC_FORMAT);
        uint32_t adjustedInterval = MyTim->getOverflow(TICK_FORMAT) + FreqOffset;
        MyTim->setOverflow(adjustedInterval, TICK_FORMAT);
        hwTimer::PhaseShift = 0;
        hwTimer::callbackTock();
    }
    hwTimer::TickTock = !hwTimer::TickTock;
}
#endif

#ifdef PLATFORM_STM32
#include "STM32_hwTimer.h"

void inline hwTimer::nullCallback(void) {}

void (*hwTimer::callbackTick)() = &nullCallback;
void (*hwTimer::callbackTock)() = &nullCallback;

volatile uint32_t hwTimer::HWtimerInterval = TimerIntervalUSDefault;
volatile bool hwTimer::isTick = false;
volatile int32_t hwTimer::PhaseShift = 0;
volatile int32_t hwTimer::FreqOffset = 0;
volatile uint32_t hwTimer::PauseDuration = 0;
bool hwTimer::running = false;
bool hwTimer::alreadyInit = false;
bool hwTimer::isPaused = false;
bool hwTimer::PauseReq = false;

#if defined(TIM1)
HardwareTimer(*hwTimer::MyTim) = new HardwareTimer(TIM1);
#else
// FM30_mini (STM32F373xC) no advanced timer but TIM2 is 32-bit general purpose
HardwareTimer(*hwTimer::MyTim) = new HardwareTimer(TIM2);
#endif

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

void hwTimer::pause(uint32_t duration)
{
    PauseDuration = duration;
    PauseReq = true; 
    while(!isPaused){
        __NOP();
    }
}

void hwTimer::resume()
{
    isTick = false;
    running = true;
    MyTim->resume();
    MyTim->refresh();
}

void hwTimer::updateInterval(uint32_t newTimerInterval)
{
    hwTimer::HWtimerInterval = newTimerInterval;
    MyTim->setOverflow((hwTimer::HWtimerInterval >> 1), MICROSEC_FORMAT);
}

void hwTimer::resetFreqOffset()
{
    FreqOffset = 0;
}

void hwTimer::incFreqOffset()
{
    FreqOffset++;
}

void hwTimer::decFreqOffset()
{
    FreqOffset--;
}

void hwTimer::phaseShift(int32_t newPhaseShift)
{
    int32_t minVal = -(hwTimer::HWtimerInterval >> 4);
    int32_t maxVal = (hwTimer::HWtimerInterval >> 4);

    hwTimer::PhaseShift = constrain(newPhaseShift, minVal, maxVal);
}

void hwTimer::PauseDoneCallback(void)
{
    PauseReq = false;
    hwTimer::callback();
    MyTim->attachInterrupt(hwTimer::callback);
}

void hwTimer::callback(void)
{
    if (!running)
    {
        return;
    }

    if (PauseReq)
    {
        MyTim->setOverflow(PauseDuration, MICROSEC_FORMAT);
        MyTim->attachInterrupt(hwTimer::PauseDoneCallback);
        isPaused = true;
        return;
    }

    if (hwTimer::isTick)
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
    hwTimer::isTick = !hwTimer::isTick;
}
#endif

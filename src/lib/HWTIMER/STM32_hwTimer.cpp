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
#if defined(TARGET_TX)
        // The prescaler only adjusts AFTER the overflow interrupt fires so
        // to make Pause() work, the prescaler needs to be fixed to avoid
        // having to ramp between prescalers
        MyTim->setPrescaleFactor(MyTim->getTimerClkFreq() / 1000000); // 1us per tick
        MyTim->setOverflow(hwTimer::HWtimerInterval >> 1, TICK_FORMAT);
#else
        MyTim->setOverflow(hwTimer::HWtimerInterval >> 1, MICROSEC_FORMAT); // 22(50Hz) to 3(500Hz) scaler
#endif
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

/*
 * Schedule a pause of the specified duration, in us -- TX Only
 * Will pause until the TICK interrupt, then the next timer will
 * fire Duration - interval/2 after that
 * 65535us max!
 */
void hwTimer::pause(uint32_t duration)
{
#if defined(TARGET_TX)
    PauseDuration = duration;
    while(PauseDuration);
#endif
}

void hwTimer::resume()
{
    isTick = false;
    running = true;
    #if defined(TARGET_TX)
    MyTim->setOverflow(hwTimer::HWtimerInterval >> 1, TICK_FORMAT);
    #else
    MyTim->setOverflow((hwTimer::HWtimerInterval >> 1), MICROSEC_FORMAT);
    #endif
    MyTim->setCount(0);
    MyTim->resume();
    MyTim->refresh(); // will trigger the interrupt immediately, but will update the prescaler shadow reg
}

void hwTimer::updateInterval(uint32_t newTimerInterval)
{
    hwTimer::HWtimerInterval = newTimerInterval;
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
    int32_t minVal = -(hwTimer::HWtimerInterval >> 2);
    int32_t maxVal = (hwTimer::HWtimerInterval >> 2);
    hwTimer::PhaseShift = constrain(newPhaseShift, minVal, maxVal);
}

void hwTimer::callback(void)
{
    if (hwTimer::isTick)
    {
#if defined(TARGET_TX)
        if (PauseDuration)
        {
            MyTim->setOverflow(PauseDuration - (hwTimer::HWtimerInterval >> 1), TICK_FORMAT);
            PauseDuration = 0;
            // No tick callback
        }
        else
        {
            MyTim->setOverflow(hwTimer::HWtimerInterval >> 1, TICK_FORMAT);
            hwTimer::callbackTick();
        }
#else
        MyTim->setOverflow((hwTimer::HWtimerInterval >> 1), MICROSEC_FORMAT);
        uint32_t adjustedInterval = MyTim->getOverflow(TICK_FORMAT) + FreqOffset;
        MyTim->setOverflow(adjustedInterval, TICK_FORMAT);
        hwTimer::callbackTick();
#endif
    }
    else
    {
#if defined(TARGET_TX)
        MyTim->setOverflow(hwTimer::HWtimerInterval >> 1, TICK_FORMAT);
        hwTimer::callbackTock();
#else
        MyTim->setOverflow((hwTimer::HWtimerInterval >> 1) + hwTimer::PhaseShift, MICROSEC_FORMAT);
        uint32_t adjustedInterval = MyTim->getOverflow(TICK_FORMAT) + FreqOffset;
        MyTim->setOverflow(adjustedInterval, TICK_FORMAT);
        hwTimer::PhaseShift = 0;
        hwTimer::callbackTock();
#endif
    }
    hwTimer::isTick = !hwTimer::isTick;
}
#endif

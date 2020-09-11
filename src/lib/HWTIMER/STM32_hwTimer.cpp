#ifdef PLATFORM_STM32
#include "STM32_hwTimer.h"

void inline hwTimer::nullCallback(void){};

void (*hwTimer::callbackTick)() = &nullCallback; // function is called whenever there is new RC data.
void (*hwTimer::callbackTock)() = &nullCallback; // function is called whenever there is new RC data.

volatile uint32_t hwTimer::HWtimerInterval = TimerIntervalUSDefault;
volatile bool hwTimer::TickTock = true;
volatile int32_t hwTimer::PhaseShift = 0;
volatile int32_t hwTimer::FreqShift = 0;
volatile bool hwTimer::ResetNextLoop = false;
bool hwTimer::running = false;
bool hwTimer::alreadyInit = false;

volatile uint32_t hwTimer::LastCallbackMicrosTick = 0;
volatile uint32_t hwTimer::LastCallbackMicrosTock = 0;

HardwareTimer(*hwTimer::MyTim) = new HardwareTimer(TIM1);

void hwTimer::init()
{
    if (!alreadyInit)
    {
        MyTim->attachInterrupt(hwTimer::callback);
        MyTim->setMode(1, TIMER_OUTPUT_COMPARE);
        MyTim->setOverflow(hwTimer::HWtimerInterval >> 1, MICROSEC_FORMAT);
        MyTim->setPreloadEnable(true);
        alreadyInit = true;
    }
}

void hwTimer::stop()
{
    running = false;
    MyTim->pause();
    MyTim->setCount(0);
    TickTock = true;
}

void hwTimer::resume()
{
    TickTock = true;
    running = true;
    MyTim->resume();
}

void hwTimer::updateInterval(uint32_t newTimerInterval)
{
    hwTimer::HWtimerInterval = newTimerInterval;
    MyTim->setOverflow(hwTimer::HWtimerInterval >> 1, MICROSEC_FORMAT);
}

void hwTimer::phaseShift(int32_t newPhaseShift)
{
    //Serial.println(newPhaseShift);
    int32_t MaxPhaseShift = hwTimer::HWtimerInterval >> 2;

    if (newPhaseShift > MaxPhaseShift)
    {
        hwTimer::PhaseShift = MaxPhaseShift;
    }
    else if (newPhaseShift < -MaxPhaseShift)
    {
        hwTimer::PhaseShift = -MaxPhaseShift;
    }
    else
    {
        hwTimer::PhaseShift = newPhaseShift;
    }
}

void hwTimer::callback(void)
{
    if (!running)
    {
        return;
    }

    if (hwTimer::TickTock)
    {
        if (hwTimer::ResetNextLoop)
        {
            MyTim->setOverflow(hwTimer::HWtimerInterval >> 1, MICROSEC_FORMAT);
            hwTimer::ResetNextLoop = false;
        }

        if (hwTimer::PhaseShift > 1 || hwTimer::PhaseShift < 1)
        {
            MyTim->setOverflow((hwTimer::HWtimerInterval >> 1) + hwTimer::PhaseShift, MICROSEC_FORMAT);

            hwTimer::ResetNextLoop = true;
            hwTimer::PhaseShift = 0;
        }
        hwTimer::LastCallbackMicrosTick = micros();
        hwTimer::callbackTick();
    }
    else
    {
        hwTimer::LastCallbackMicrosTock = micros();
        hwTimer::callbackTock();
    }
    hwTimer::TickTock = !hwTimer::TickTock;
}
#endif

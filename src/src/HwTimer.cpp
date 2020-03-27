#include "HwTimer.h"

/* HW specific code can be found from <mcu type>/ folder */

void (*HwTimer::callbackTick)() = &nullCallback; // function is called whenever there is new RC data.
void (*HwTimer::callbackTock)() = &nullCallback; // function is called whenever there is new RC data.

volatile uint32_t HwTimer::HWtimerInterval = TimerIntervalUSDefault;
volatile bool HwTimer::TickTock = false;
volatile int16_t HwTimer::PhaseShift = 0;
volatile bool HwTimer::ResetNextLoop = false;

volatile uint32_t HwTimer::LastCallbackMicrosTick = 0;
volatile uint32_t HwTimer::LastCallbackMicrosTock = 0;

void HwTimer::updateInterval(uint32_t newTimerInterval)
{
#ifdef PLATFORM_STM32
    HwTimer::HWtimerInterval = newTimerInterval;
#else
    HwTimer::HWtimerInterval = newTimerInterval * 5;
#endif
    HwTimer::setTime(HwTimer::HWtimerInterval >> 1);
}

void ICACHE_RAM_ATTR HwTimer::phaseShift(int32_t newPhaseShift)
{
    //Serial.println(newPhaseShift);
    int32_t MaxPhaseShift = HwTimer::HWtimerInterval >> 1;

    if (newPhaseShift > MaxPhaseShift)
    {
        HwTimer::PhaseShift = MaxPhaseShift;
    }
    else
    {
        HwTimer::PhaseShift = newPhaseShift;
    }

    if (newPhaseShift < -MaxPhaseShift)
    {
        HwTimer::PhaseShift = -MaxPhaseShift;
    }
    else
    {
        HwTimer::PhaseShift = newPhaseShift;
    }
#ifndef PLATFORM_STM32
    HwTimer::PhaseShift = HwTimer::PhaseShift * 5;
#endif
}

void ICACHE_RAM_ATTR HwTimer::callback()
{

    if (HwTimer::TickTock)
    {
        if (HwTimer::ResetNextLoop)
        {

            HwTimer::setTime(HwTimer::HWtimerInterval >> 1);
            HwTimer::ResetNextLoop = false;
        }

        if (HwTimer::PhaseShift > 0 || HwTimer::PhaseShift < 0)
        {

            HwTimer::setTime((HwTimer::HWtimerInterval + HwTimer::PhaseShift) >> 1);

            HwTimer::ResetNextLoop = true;
            HwTimer::PhaseShift = 0;
        }

        HwTimer::LastCallbackMicrosTick = micros();
        HwTimer::callbackTick();
    }
    else
    {
        HwTimer::LastCallbackMicrosTock = micros();
        HwTimer::callbackTock();
    }
    HwTimer::TickTock = !HwTimer::TickTock;
}

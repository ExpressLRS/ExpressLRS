#include "HwTimer.h"

/* HW specific code can be found from <mcu type>/ folder */

void (*HwTimer::callbackTick)() = &nullCallback; // function is called whenever there is new RC data.
void (*HwTimer::callbackTock)() = &nullCallback; // function is called whenever there is new RC data.

volatile uint32_t HwTimer::HWtimerInterval = TimerIntervalUSDefault;
volatile bool HwTimer::TickTock = 0;
volatile int32_t HwTimer::PhaseShift = 0;
volatile bool HwTimer::ResetNextLoop = false;

volatile uint32_t HwTimer::LastCallbackMicrosTick = 0;
volatile uint32_t HwTimer::LastCallbackMicrosTock = 0;

void HwTimer::updateInterval(uint32_t newTimerInterval)
{
    HWtimerInterval = newTimerInterval;
    setTime(newTimerInterval >> 1);
}

void ICACHE_RAM_ATTR HwTimer::phaseShift(int32_t newPhaseShift)
{
    int32_t const MaxPhaseShift = HWtimerInterval >> 1;

    if (newPhaseShift > MaxPhaseShift)
    {
        PhaseShift = MaxPhaseShift;
    }
    else if (newPhaseShift < -MaxPhaseShift)
    {
        PhaseShift = -MaxPhaseShift;
    }
    else
    {
        PhaseShift = newPhaseShift;
    }
}

void ICACHE_RAM_ATTR HwTimer::callback()
{

    if (TickTock)
    {
#if 0
        if (ResetNextLoop)
        {

            setTime(HWtimerInterval >> 1);
            ResetNextLoop = false;
        }

        if (PhaseShift != 0)
        {

            setTime((HWtimerInterval + PhaseShift) >> 1);

            ResetNextLoop = true;
            PhaseShift = 0;
        }
#else
        setTime((HWtimerInterval + PhaseShift) >> 1);
        PhaseShift = 0;
#endif
        LastCallbackMicrosTick = micros();
        callbackTick();
    }
    else
    {
        LastCallbackMicrosTock = micros();
        callbackTock();
    }
    TickTock ^= 1;
}

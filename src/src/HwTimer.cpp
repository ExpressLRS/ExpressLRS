#include "HwTimer.h"

/* HW specific code can be found from <mcu type>/ folder */

HwTimer::HwTimer()
{
    callbackTick = nullCallback; // function is called whenever there is new RC data.
    callbackTick = nullCallback; // function is called whenever there is new RC data.

    HWtimerInterval = TimerIntervalUSDefault;
    TickTock = 0;
    PhaseShift = 0;
    ResetNextLoop = false;

    LastCallbackMicrosTick = 0;
    LastCallbackMicrosTock = 0;
}

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

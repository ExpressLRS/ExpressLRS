#include "HwTimer.h"
#include "debug.h"
#include <Arduino.h>

/* HW specific code can be found from <mcu type>/ folder */

static void nullCallback(uint32_t){};

HwTimer::HwTimer()
{
    callbackTock = nullCallback; // function is called whenever there is new RC data.

    HWtimerInterval = TimerIntervalUSDefault;
}

void ICACHE_RAM_ATTR HwTimer::updateInterval(uint32_t newTimerInterval)
{
    HWtimerInterval = newTimerInterval;
    //setTime(newTimerInterval);
}

void ICACHE_RAM_ATTR HwTimer::callback()
{
    uint32_t us = micros();
    callbackTock(us);
}

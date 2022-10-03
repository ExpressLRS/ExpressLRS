#pragma once

#include "targets.h"
#include <stdio.h>

#if defined(TARGET_TX)
#define TimerIntervalUSDefault 4000
#else
#define TimerIntervalUSDefault 20000
#endif

class hwTimer
{
  public:
    static volatile bool running;
    static volatile bool isTick;

    static void init();
    static void stop();
    static void resume();
    static void updateInterval(uint32_t time = TimerIntervalUSDefault);
    static void resetFreqOffset();
    static void incFreqOffset();
    static void decFreqOffset();
    static void phaseShift(int32_t newPhaseShift);
#if defined(PLATFORM_STM32)
    static void pause(uint32_t duration);
#endif

    static void (*callbackTick)();
    static void (*callbackTock)();

  private:
    static void callback();

    static volatile uint32_t HWtimerInterval;
    static volatile int32_t PhaseShift;
    static volatile int32_t FreqOffset;
};

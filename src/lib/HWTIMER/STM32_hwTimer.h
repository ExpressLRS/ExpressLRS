#pragma once

#include <stdio.h>
#include "targets.h"

#define TimerIntervalUSDefault 20000


class hwTimer
{
public:
    static HardwareTimer *MyTim;

    static volatile uint32_t HWtimerInterval;
    static volatile bool isTick;
    static volatile bool SkipCallback;
    static volatile int32_t PhaseShift;
    static volatile int32_t FreqOffset;
    static volatile uint32_t PauseDuration;
    static bool running;
    static bool alreadyInit;

    static void init();
    static void stop();
    static void pause(uint32_t duration);
    static void resume();
    static void callback(void);
    static void updateInterval(uint32_t newTimerInterval);
    static void resetFreqOffset();
    static void incFreqOffset();
    static void decFreqOffset();
    static void phaseShift(int32_t newPhaseShift);

    static void inline nullCallback(void);
    static void (*callbackTick)();
    static void (*callbackTock)();
};
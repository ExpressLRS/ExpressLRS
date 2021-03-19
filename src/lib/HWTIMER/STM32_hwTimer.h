#pragma once

#include <Arduino.h>
#include <stdio.h>
#include "../../src/targets.h"

#define TimerIntervalUSDefault 20000


class hwTimer
{
public:
    static HardwareTimer *MyTim;

    static volatile uint32_t HWtimerInterval;
    static volatile bool TickTock;
    static volatile int32_t PhaseShift;
    static volatile int32_t FreqOffset;
    static bool running;
    static bool alreadyInit;

    static void init();
    static void stop();
    static void resume();
    static void callback(void);
    static void updateInterval(uint32_t newTimerInterval);
    static void incFreqOffset();
    static void decFreqOffset();
    static void phaseShift(int32_t newPhaseShift);

    static void inline nullCallback(void);
    static void (*callbackTick)();
    static void (*callbackTock)();
};
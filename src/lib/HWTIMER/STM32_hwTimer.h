#pragma once

#include <stdio.h>
#include "targets.h"

#define TimerIntervalUSDefault 20000

//HardwareTimer *MyTim;

class hwTimer
{
public:
    //static TIM_TypeDef *Instance;
    static HardwareTimer *MyTim;

    static volatile uint32_t HWtimerInterval;
    static volatile bool TickTock;
    static volatile int32_t PhaseShift;
    static volatile int32_t FreqShift;
    static volatile bool ResetNextLoop;
    static bool running;
    static bool alreadyInit;
    static volatile uint32_t LastCallbackMicrosTick;
    static volatile uint32_t LastCallbackMicrosTock;

    static void init();
    static void stop();
    static void resume();
    static void callback(void);

    static void updateInterval(uint32_t newTimerInterval);
    static void phaseShift(int32_t newPhaseShift);

    static void inline nullCallback();
    static void (*callbackTick)();
    static void (*callbackTock)();
};
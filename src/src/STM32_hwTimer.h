#pragma once

#include <Arduino.h>
#include <stdio.h>
#include "../../src/targets.h"

#define TimerIntervalUSDefault 20000

//HardwareTimer *MyTim;

class hwTimer
{
public:
    //static TIM_TypeDef *Instance;
    static HardwareTimer *MyTim;

    static volatile uint32_t HWtimerInterval;
    static volatile bool TickTock;
    static volatile int16_t PhaseShift;
    static volatile bool ResetNextLoop;
    static volatile uint32_t LastCallbackMicrosTick;
    static volatile uint32_t LastCallbackMicrosTock;

    static void init();
    static void pause();
    static void stop();
    static void callback(HardwareTimer *);
    static void updateInterval(uint32_t newTimerInterval);
    static void phaseShift(int32_t newPhaseShift);

    static void inline nullCallback();
    static void (*callbackTick)();
    static void (*callbackTock)();
};
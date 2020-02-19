#pragma once

#include "../../src/targets.h"
#include <Arduino.h>
#include <stdio.h>

#define TimerIntervalUSDefault 5000

//HardwareTimer *MyTim;

class hwTimer
{
public:
    //static TIM_TypeDef *Instance;
    static HardwareTimer *MyTim;

    static volatile uint32_t HWtimerInterval;
    static bool TickTock;
    static volatile int32_t PhaseShift;
    static bool ResetNextLoop;
    static uint32_t LastCallbackMicrosTick;
    static uint32_t LastCallbackMicrosTock;
    static uint8_t TimerDiv;
    static uint8_t Counter;

    static uint8_t Prescaler;
    static bool UpdatePrescaler;

    static bool NewIntervalReq;
    static volatile uint32_t newHWtimerInterval;

    static void init();
    static void ICACHE_RAM_ATTR pause();
    static void ICACHE_RAM_ATTR stop();
    static void ICACHE_RAM_ATTR setPrescaler(uint32_t newPrescaler);
    static void ICACHE_RAM_ATTR callback(HardwareTimer *);
    static void ICACHE_RAM_ATTR updateInterval(uint32_t newTimerInterval);
    static void ICACHE_RAM_ATTR phaseShift(int32_t newPhaseShift);
    static void ICACHE_RAM_ATTR phaseShiftNoLimit(int32_t newPhaseShift);

    static void inline nullCallback();
    static void (*callbackTick)();
    static void (*callbackTock)();
};
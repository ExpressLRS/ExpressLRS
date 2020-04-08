#pragma once

#include <Arduino.h>
#include <stdio.h>

#define TimerIntervalUSDefault 20000

class HwTimer
{
public:
    volatile uint32_t LastCallbackMicrosTick;
    volatile uint32_t LastCallbackMicrosTock;

    HwTimer();
    void init();
    void ICACHE_RAM_ATTR start();
    void ICACHE_RAM_ATTR pause();
    void ICACHE_RAM_ATTR stop();
    void ICACHE_RAM_ATTR updateInterval(uint32_t newTimerInterval);
    void ICACHE_RAM_ATTR phaseShift(int32_t newPhaseShift);

    void ICACHE_RAM_ATTR callback();

    static inline void nullCallback(){};
    void (*callbackTick)();
    void (*callbackTock)();

private:
    volatile uint32_t HWtimerInterval;
    volatile bool TickTock;
    volatile int32_t PhaseShift;
    volatile bool ResetNextLoop;

    void setTime(uint32_t);
};

extern HwTimer TxTimer;

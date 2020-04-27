#pragma once

#include "platform.h"
#include <stdint.h>

#define PRINT_TIMER 0

#define TimerIntervalUSDefault 20000

class HwTimer
{
public:
    HwTimer();
    void init();
    void ICACHE_RAM_ATTR start();
    void ICACHE_RAM_ATTR reset(int32_t offset = 0);
    void ICACHE_RAM_ATTR pause();
    void ICACHE_RAM_ATTR stop();
    void ICACHE_RAM_ATTR updateInterval(uint32_t newTimerInterval);
    bool ICACHE_RAM_ATTR isRunning(void)
    {
        return running;
    }

    void ICACHE_RAM_ATTR callback();

    void (*callbackTock)(uint32_t us);

    void ICACHE_RAM_ATTR setTime(uint32_t time = 0);

private:
    volatile uint32_t HWtimerInterval;
    volatile bool running = false;

    //void ICACHE_RAM_ATTR setTime(uint32_t);
};

extern HwTimer TxTimer;

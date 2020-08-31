#pragma once

#include <Arduino.h>
#include <stdio.h>
#include "../../src/targets.h"

#define TimerIntervalUSDefault 4000

class hwTimer
{
public:
    static void init();
    static void ICACHE_RAM_ATTR resume();
    static void ICACHE_RAM_ATTR stop();
    static void ICACHE_RAM_ATTR updateInterval(uint32_t time = TimerIntervalUSDefault);

    static void (*callbackTock)();
    static void ICACHE_RAM_ATTR nullCallback(void);
    static void ICACHE_RAM_ATTR callback();

    static volatile uint32_t HWtimerInterval;
    static volatile bool running;

};
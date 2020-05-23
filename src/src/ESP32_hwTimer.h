  
#pragma once

#include <stdint.h>
#include "targets.h"
#include "LoRa_SX127x.h"
#include <FreeRTOS.h>
#include <esp32-hal-timer.h>
#include "debug.h"

#define TimerIntervalUSDefault 5000

class hwTimer
{
public:
    static void init();
    static void ICACHE_RAM_ATTR start();
    static void ICACHE_RAM_ATTR stop();
    static void ICACHE_RAM_ATTR pause();
    static void ICACHE_RAM_ATTR updateInterval(uint32_t time = 0);

    static void (*callbackTock)();
    static void ICACHE_RAM_ATTR nullCallback(void);
    static void ICACHE_RAM_ATTR callback();

    static volatile uint32_t HWtimerInterval;
    static volatile bool running;

};
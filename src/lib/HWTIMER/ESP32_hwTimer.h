#pragma once

#include <stdio.h>
#include "targets.h"

#if defined(TARGET_TX)
#define TimerIntervalUSDefault 4000
#else
#define TimerIntervalUSDefault 20000
#endif

class hwTimer
{
public:
    static volatile uint32_t HWtimerInterval;
    static volatile bool running;
#if defined(TARGET_RX)
	static volatile bool isTick;
	static volatile int32_t PhaseShift;
	static volatile int32_t FreqOffset;
	static uint32_t NextTimeout;
#endif

    static void init();
    static void stop();
    static void resume();
    static void callback();
    static void updateInterval(uint32_t time = TimerIntervalUSDefault);
#if defined(TARGET_RX)
	static void resetFreqOffset();
	static void incFreqOffset();
	static void decFreqOffset();
	static void phaseShift(int32_t newPhaseShift);
#endif

    static void nullCallback(void);
#if defined(TARGET_RX)
	static void (*callbackTick)();
#endif
    static void (*callbackTock)();
};
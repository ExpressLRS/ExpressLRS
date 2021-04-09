#pragma once

#include <stdio.h>
#include "targets.h"

#define TimerIntervalUSDefault 20000

extern "C"
{
#include "user_interface.h"
}

class hwTimer
{
public:
	static volatile uint32_t HWtimerInterval;
	static volatile bool TickTock;
	static volatile int32_t PhaseShift;
	static volatile int32_t FreqOffset;
	static bool running;

	static void init();
	static void ICACHE_RAM_ATTR stop();
	static void ICACHE_RAM_ATTR resume();
	static void ICACHE_RAM_ATTR callback();
	static void ICACHE_RAM_ATTR updateInterval(uint32_t newTimerInterval);
	static void ICACHE_RAM_ATTR incFreqOffset();
	static void ICACHE_RAM_ATTR decFreqOffset();
	static void ICACHE_RAM_ATTR phaseShift(int32_t newPhaseShift);

	static void inline nullCallback(void);
	static void (*callbackTick)();
	static void (*callbackTock)();
};
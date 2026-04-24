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
	static bool running;
	static volatile bool isTick;
	static volatile int32_t PhaseShift;
	static volatile int32_t FreqOffset;
	static uint32_t NextTimeout;

	static void init();
	static void stop();
	static void resume();
	static void callback();
	static void updateInterval(uint32_t newTimerInterval);
	static void resetFreqOffset();
	static void incFreqOffset();
	static void decFreqOffset();
	static void phaseShift(int32_t newPhaseShift);

	static void inline nullCallback(void);
	static void (*callbackTick)();
	static void (*callbackTock)();
};
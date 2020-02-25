#pragma once

#include "Arduino.h"
#include "targets.h"

#ifdef PLATFORM_STM32
//
#else
extern "C"
{
#include "user_interface.h"
}
#endif

//////////Hardware Timer Functions//////////////////////////////////////
void inline nullCallback(void){};
void (*HWtimerCallBack)(void) = nullCallback;
void (*HWtimerCallBack90)(void) = nullCallback;
#define HardwareTimerBaseInterval 1000;
#define TimerIntervalUSDefault 16000
volatile uint32_t HWtimerInterval = TimerIntervalUSDefault;
volatile uint32_t HWtimerIntervalUS;
volatile bool TickTock = false;
volatile uint32_t HWtimerLastCallbackMicros;
volatile uint32_t HWtimerLastCallbackMicros90;
uint32_t ICACHE_RAM_ATTR HWtimerGetlastCallbackMicros();
uint32_t ICACHE_RAM_ATTR HWtimerGetlastCallbackMicros90();
uint32_t ICACHE_RAM_ATTR HWtimerGetIntervalMicros();
volatile int16_t PhaseShift = 0;
bool ResetNextLoop = false;

void ICACHE_RAM_ATTR HWtimerUpdateInterval(uint32_t _TimerInterval);
void ICACHE_RAM_ATTR HWtimerPhaseShift(int16_t Offset);
void ICACHE_RAM_ATTR Timer0Callback();
void InitHarwareTimer();
void StopHWtimer();
void HWtimerSetCallback(void (*CallbackFunc)(void));
void HWtimerSetCallback90(void (*CallbackFunc)(void));
////////////////////////////////////////////////////////////////////////
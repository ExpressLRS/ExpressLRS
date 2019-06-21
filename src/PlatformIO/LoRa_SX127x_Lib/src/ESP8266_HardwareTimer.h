#ifndef ESP8266_HARDWARE_TIMERS_H
#define ESP8266_HARDWARE_TIMERS_H

#include "Arduino.h"

extern "C"
{
#include "user_interface.h"
}

//////////Hardware Timer Functions//////////////////////////////////////
void (*HWtimerCallBack)(void) = NULL;
void (*HWtimerCallBack90)(void) = NULL;
#define HardwareTimerBaseInterval 1000;
#define TimerIntervalUSDefault 16000
volatile uint32_t HWtimerInterval = TimerIntervalUSDefault;
volatile bool TickTock = false;
uint32_t HWtimerLastCallbackMicros;
uint32_t HWtimerLastCallbackMicros90;
uint32_t ICACHE_RAM_ATTR HWtimerGetlastCallbackMicros();
uint32_t ICACHE_RAM_ATTR HWtimerGetlastCallbackMicros90();
uint32_t ICACHE_RAM_ATTR HWtimerGetIntervalMicros();
int16_t PhaseShift = 0;
bool ResetNextLoop = false;

void ICACHE_RAM_ATTR HWtimerUpdateInterval(uint32_t _TimerInterval);
void ICACHE_RAM_ATTR HWtimerPhaseShift(int16_t Offset);
void ICACHE_RAM_ATTR Timer0Callback();
void InitHarwareTimer();
void  StopHWtimer();
void HWtimerSetCallback(void (*CallbackFunc)(void));
void HWtimerSetCallback90(void (*CallbackFunc)(void));
////////////////////////////////////////////////////////////////////////
#endif
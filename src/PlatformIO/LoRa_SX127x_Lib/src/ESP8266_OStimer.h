#ifndef ESP8266_SOFTWARE_TIMERS_H
#define ESP8266_SOFTWARE_TIMERS_H

#include "Arduino.h"

extern "C"
{
#include "user_interface.h"
}

/////////////OS (Software Timer)//////////////////////////////////
void (*OStimerCallBack)(void) = NULL;
os_timer_t OStimer;
#define OStimerIntervalUSDefault 16000
volatile uint32_t OStimerInterval = OStimerIntervalUSDefault;

void ICACHE_RAM_ATTR OStimerCallback(void *pArg);
void InitOStimer();
void OStimerSetCallback(void (*CallbackFunc)(void));
void ICACHE_RAM_ATTR OStimerReset();
void ICACHE_RAM_ATTR OStimerUpdateInterval(uint32_t Interval);
/////////////////////////////////////////////////////////////////////////

#endif
#include "Arduino.h"
#include "ESP8266_OStimer.h"

void ICACHE_RAM_ATTR OStimerCallback(void *pArg)
{
   OStimerCallBack();
}

void InitOStimer()
{
    system_timer_reinit();
    os_timer_setfn(&OStimer, OStimerCallback, NULL);
    ets_timer_arm_new(&OStimer, OStimerInterval, true, 0);
}

void OStimerSetCallback(void (*CallbackFunc)(void))
{
    OStimerCallBack = CallbackFunc;
}

void ICACHE_RAM_ATTR OStimerReset()
{
    os_timer_disarm(&OStimer);
    os_timer_setfn(&OStimer, OStimerCallback, NULL);
    os_timer_arm(&OStimer, OStimerInterval, 0);
}

void ICACHE_RAM_ATTR OStimerUpdateInterval(uint32_t Interval)
{
    OStimerInterval = Interval;
}

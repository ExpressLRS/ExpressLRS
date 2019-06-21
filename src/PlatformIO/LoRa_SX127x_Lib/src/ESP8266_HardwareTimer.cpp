#include "Arduino.h"
#include "ESP8266_HardwareTimer.h"

uint32_t ICACHE_RAM_ATTR HWtimerGetlastCallbackMicros()
{
    return HWtimerLastCallbackMicros;
}

uint32_t ICACHE_RAM_ATTR HWtimerGetlastCallbackMicros90()
{
    return HWtimerLastCallbackMicros90;
}

uint32_t ICACHE_RAM_ATTR HWtimerGetIntervalMicros()
{
    return HWtimerInterval;
}

void ICACHE_RAM_ATTR HWtimerUpdateInterval(uint32_t _TimerInterval)
{
    HWtimerInterval = _TimerInterval * 5;
    timer1_write(HWtimerInterval / 2);
}

void ICACHE_RAM_ATTR HWtimerPhaseShift(int16_t Offset)
{
    PhaseShift = Offset * 5;
}

void ICACHE_RAM_ATTR Timer0Callback()
{
    if (TickTock)
    {

        if (ResetNextLoop)
        {
            timer1_write(HWtimerInterval / 2);
            ResetNextLoop = false;
        }

        if (PhaseShift > 0 || PhaseShift < 0)
        {
            timer1_write((HWtimerInterval + PhaseShift) / 2);
            ResetNextLoop = true;
            PhaseShift = 0;
        }

        //uint32_t next = ESP.getCycleCount() + HWtimerInterval * 5;
        //timer1_write(next + PhaseShift); // apply phase shift to next cycle
        //PhaseShift = 0; //then reset the phase shift variable
        HWtimerCallBack();
        HWtimerLastCallbackMicros = micros();
    }
    else
    {
        HWtimerCallBack90();
        HWtimerLastCallbackMicros90 = micros();
    }
    TickTock = !TickTock;
}

void ICACHE_RAM_ATTR InitHarwareTimer()
{
    noInterrupts();
    timer1_attachInterrupt(Timer0Callback);
    timer1_enable(TIM_DIV16, TIM_EDGE, TIM_LOOP); //5MHz ticks
    timer1_write(HWtimerInterval);                //120000 us
    interrupts();
}

void StopHWtimer()
{
    timer1_detachInterrupt();
}

void HWtimerSetCallback(void (*CallbackFunc)(void))
{
    HWtimerCallBack = CallbackFunc;
}

void HWtimerSetCallback90(void (*CallbackFunc)(void))
{
    HWtimerCallBack90 = CallbackFunc;
}
#ifdef PLATFORM_ESP8266
#include "ESP8266_hwTimer.h"

void inline hwTimer::nullCallback(void){};

void (*hwTimer::callbackTick)() = &nullCallback;
void (*hwTimer::callbackTock)() = &nullCallback;

volatile uint32_t hwTimer::HWtimerInterval = TimerIntervalUSDefault;
volatile bool hwTimer::TickTock = true;
volatile int16_t hwTimer::PhaseShift = 0;
bool hwTimer::ResetNextLoop = false;
bool hwTimer::running = false;

uint32_t hwTimer::LastCallbackMicrosTick = 0;
uint32_t hwTimer::LastCallbackMicrosTock = 0;

void hwTimer::init()
{
    if (!running)
    {
        timer1_attachInterrupt(hwTimer::callback);
        timer1_enable(TIM_DIV16, TIM_EDGE, TIM_LOOP); //5MHz ticks
        timer1_write(hwTimer::HWtimerInterval >> 1);  //120000 us
        ResetNextLoop = false;
        TickTock = true;
        running = true;
    }
}

void hwTimer::stop()
{
    if (running)
    {
        timer1_disable();
        timer1_detachInterrupt();
        running = false;
    }
}

void hwTimer::resume()
{
    if (!running)
    {
        init();
        running = true;
    }
}

void hwTimer::updateInterval(uint32_t newTimerInterval)
{
    hwTimer::HWtimerInterval = newTimerInterval * 5;
    if (running)
    {
        timer1_write(hwTimer::HWtimerInterval >> 1);
    }
}

void ICACHE_RAM_ATTR hwTimer::phaseShift(int32_t newPhaseShift)
{
    int32_t MaxPhaseShift = hwTimer::HWtimerInterval >> 1;

    if (newPhaseShift > MaxPhaseShift)
    {
        hwTimer::PhaseShift = MaxPhaseShift;
    }
    else
    {
        hwTimer::PhaseShift = newPhaseShift;
    }

    if (newPhaseShift < -MaxPhaseShift)
    {
        hwTimer::PhaseShift = -MaxPhaseShift;
    }
    else
    {
        hwTimer::PhaseShift = newPhaseShift;
    }
    hwTimer::PhaseShift = hwTimer::PhaseShift * 5;
}

void ICACHE_RAM_ATTR hwTimer::callback()
{

    if (hwTimer::TickTock)
    {
        if (hwTimer::ResetNextLoop)
        {

            timer1_write(hwTimer::HWtimerInterval >> 1);
            hwTimer::ResetNextLoop = false;
        }

        if (hwTimer::PhaseShift > 1 || hwTimer::PhaseShift < 1)
        {

            timer1_write((hwTimer::HWtimerInterval >> 1) + hwTimer::PhaseShift);

            hwTimer::ResetNextLoop = true;
            hwTimer::PhaseShift = 0;
        }

        hwTimer::LastCallbackMicrosTick = micros();
        hwTimer::callbackTick();
    }
    else
    {
        hwTimer::LastCallbackMicrosTock = micros();
        hwTimer::callbackTock();
    }
    hwTimer::TickTock = !hwTimer::TickTock;
}
#endif

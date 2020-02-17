#include "ESP8266_hwTimer.h"

void inline hwTimer::nullCallback(void){};

void (*hwTimer::callbackTick)() = &nullCallback; // function is called whenever there is new RC data.
void (*hwTimer::callbackTock)() = &nullCallback; // function is called whenever there is new RC data.

volatile uint32_t hwTimer::HWtimerInterval = TimerIntervalUSDefault;
volatile bool hwTimer::TickTock = false;
volatile int16_t hwTimer::PhaseShift = 0;
bool hwTimer::ResetNextLoop = false;

uint32_t hwTimer::LastCallbackMicrosTick = 0;
uint32_t hwTimer::LastCallbackMicrosTock = 0;

void hwTimer::init()
{
    noInterrupts();
    timer1_attachInterrupt(hwTimer::callback);
    timer1_enable(TIM_DIV16, TIM_EDGE, TIM_LOOP); //5MHz ticks
    timer1_write(hwTimer::HWtimerInterval);       //120000 us
    interrupts();
}

void hwTimer::stop()
{
    timer1_detachInterrupt();
}

void hwTimer::pause()
{
    timer1_detachInterrupt();
}

void hwTimer::updateInterval(uint32_t newTimerInterval)
{
    hwTimer::HWtimerInterval = newTimerInterval * 5;
    timer1_write(hwTimer::HWtimerInterval >> 1);
}

void ICACHE_RAM_ATTR hwTimer::phaseShift(uint32_t newPhaseShift)
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

        if (hwTimer::PhaseShift > 0 || hwTimer::PhaseShift < 0)
        {

            timer1_write((hwTimer::HWtimerInterval + hwTimer::PhaseShift) >> 1);

            hwTimer::ResetNextLoop = true;
            hwTimer::PhaseShift = 0;
        }

        hwTimer::callbackTick();
        hwTimer::LastCallbackMicrosTick = micros();
    }
    else
    {
        hwTimer::callbackTock();
        hwTimer::LastCallbackMicrosTock = micros();
    }
    hwTimer::TickTock = !hwTimer::TickTock;
}
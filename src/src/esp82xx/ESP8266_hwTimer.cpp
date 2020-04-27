#include "HwTimer.h"
#include "targets.h"
#include "debug.h"
#include <user_interface.h>
#include <Arduino.h>

HwTimer TxTimer;

void ICACHE_RAM_ATTR MyTimCallback(void)
{
    TxTimer.callback();
}

void HwTimer::init()
{
    noInterrupts();
    timer1_attachInterrupt(MyTimCallback);
    //timer1_enable(TIM_DIV16, TIM_EDGE, TIM_LOOP); //5MHz ticks
    //setTime(HWtimerInterval);
    running = false;
    interrupts();
}

void ICACHE_RAM_ATTR HwTimer::start()
{
    noInterrupts();
    running = true;
    //timer1_attachInterrupt(MyTimCallback);
    reset(100);
    timer1_enable(TIM_DIV16, TIM_EDGE, TIM_LOOP); //5MHz ticks
    interrupts();
}

void ICACHE_RAM_ATTR HwTimer::stop()
{
    noInterrupts();
    running = false;
    timer1_disable();
    interrupts();
}

void ICACHE_RAM_ATTR HwTimer::pause()
{
    stop();
}

void ICACHE_RAM_ATTR HwTimer::reset(int32_t offset)
{
    if (running)
    {
        setTime(HWtimerInterval - offset);
    }
}

void ICACHE_RAM_ATTR HwTimer::setTime(uint32_t time)
{
    if (!time)
        return;
    timer1_write(time * 5);
#if PRINT_TIMER
    DEBUG_PRINT(" set: ");
    DEBUG_PRINT(time);
#endif
}

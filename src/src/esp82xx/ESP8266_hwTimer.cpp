#include "HwTimer.h"
#include "user_interface.h"

void HwTimer::init()
{
    noInterrupts();
    timer1_attachInterrupt(HwTimer::callback);
    timer1_enable(TIM_DIV16, TIM_EDGE, TIM_LOOP); //5MHz ticks
    timer1_write(HwTimer::HWtimerInterval);       //120000 us
    interrupts();
}

void HwTimer::stop()
{
    timer1_detachInterrupt();
}

void HwTimer::pause()
{
    timer1_detachInterrupt();
}

void HwTimer::setTime(uint32_t time)
{
    timer1_write(time);
}

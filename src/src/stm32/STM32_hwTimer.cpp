#include "HwTimer.h"

static HardwareTimer MyTim(TIM1);

void MyTimCallback(HardwareTimer *)
{
    HwTimer::callback();
}

void HwTimer::init()
{
    noInterrupts();
    MyTim.attachInterrupt(MyTimCallback);
    //MyTim.setMode(2, TIMER_OUTPUT_COMPARE);
    HwTimer::setTime(HwTimer::HWtimerInterval >> 1);
    MyTim.resume();
    interrupts();
}

void HwTimer::stop()
{
    MyTim.pause();
}

void HwTimer::pause()
{
    MyTim.pause();
}

void HwTimer::setTime(uint32_t time)
{
    MyTim.setOverflow(time, MICROSEC_FORMAT);
}

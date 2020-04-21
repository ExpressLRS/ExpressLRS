#include "HwTimer.h"
#include <Arduino.h>

HwTimer TxTimer;

static HardwareTimer timer_tx(TIM1);

static void TimerCallback(HardwareTimer *)
{
    TxTimer.callback();
}

void HwTimer::init()
{
    noInterrupts();
    timer_tx.attachInterrupt(TimerCallback);
    timer_tx.setMode(2, TIMER_OUTPUT_COMPARE);
    setTime(HWtimerInterval >> 1);
    timer_tx.resume();
    interrupts();
}

void HwTimer::start()
{
    timer_tx.resume();
}

void HwTimer::stop()
{
    timer_tx.pause();
}

void HwTimer::pause()
{
    timer_tx.pause();
}

void HwTimer::setTime(uint32_t time)
{
    timer_tx.setOverflow(time, MICROSEC_FORMAT);
}

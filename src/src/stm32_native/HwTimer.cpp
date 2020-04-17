#include "HwTimer.h"
#include "internal.h"

// CONVERT TO NATIVE STM32

#include <Arduino.h>

#if 0
HwTimer TxTimer;

TIM_HandleTypeDef handle = {
    .Instance = TIM1,
    .Channel = HAL_TIM_ACTIVE_CHANNEL_CLEARED,
    .hdma[0] = {NULL, NULL, NULL, NULL, NULL, NULL, NULL},
    .Lock = HAL_UNLOCKED,
    .State = HAL_TIM_STATE_RESET,
};

// Enable Timer clock
enableTimerClock(&(handle));

static HardwareTimer timer_tx(TIM1);

static void TimerCallback(HardwareTimer *)
{
    TxTimer.callback();
}

void HwTimer::init()
{
    noInterrupts();
    timer_tx.attachInterrupt(TimerCallback);
    //timer_tx.setMode(2, TIMER_OUTPUT_COMPARE);
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
#endif

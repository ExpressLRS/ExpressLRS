#include "HwTimer.h"
#include "debug_elrs.h"
#include <Arduino.h>

#if defined(TIM1)
#define TIMER_BASE TIM1
#elif defined(TIM2)
#define TIMER_BASE TIM2
#elif defined(TIM6)
#define TIMER_BASE TIM6
#endif

HwTimer TxTimer;

static HardwareTimer timer_tx(TIMER_BASE);

static void TimerCallback(HardwareTimer *)
{
    TxTimer.callback();
}

void HwTimer::init()
{
    noInterrupts();
    stop();
    timer_tx.attachInterrupt(TimerCallback);
    timer_tx.setMode(2, TIMER_OUTPUT_COMPARE);
    interrupts();
}

void HwTimer::start()
{
    if (running)
        return;
    running = true;
    //setTime(HWtimerInterval);
    reset(0);
    timer_tx.resume();
}

void HwTimer::stop()
{
    running = false;
    timer_tx.pause();
}

void HwTimer::pause()
{
    running = false;
    timer_tx.pause();
}

void HwTimer::reset(int32_t offset)
{
    if (running)
    {
        timer_tx.setCount(1, TICK_FORMAT);
        setTime(HWtimerInterval - offset);
    }
}

void HwTimer::setTime(uint32_t time)
{
    if (!time)
        time = HWtimerInterval;
    timer_tx.setOverflow(time, MICROSEC_FORMAT);
#if PRINT_TIMER
    DEBUG_PRINT(" set: ");
    DEBUG_PRINT(time);
#endif
}

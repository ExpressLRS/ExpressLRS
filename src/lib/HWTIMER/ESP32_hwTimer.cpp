#if defined(PLATFORM_ESP32)
#include "hwTimer.h"
#include "logging.h"

void (*hwTimer::callbackTick)() = nullptr;
void (*hwTimer::callbackTock)() = nullptr;

volatile bool hwTimer::running = false;
volatile bool hwTimer::isTick = false;

volatile uint32_t hwTimer::HWtimerInterval = TimerIntervalUSDefault;
volatile int32_t hwTimer::PhaseShift = 0;
volatile int32_t hwTimer::FreqOffset = 0;

// Internal implementation specific variables
static hw_timer_t *timer = NULL;
static portMUX_TYPE isrMutex = portMUX_INITIALIZER_UNLOCKED;

#if defined(TARGET_RX)
#define HWTIMER_TICKS_PER_US 5
#else
#define HWTIMER_TICKS_PER_US 1
#endif

void ICACHE_RAM_ATTR hwTimer::init(void (*callbackTick)(), void (*callbackTock)())
{

    if (!timer)
    {
        hwTimer::callbackTick = callbackTick;
        hwTimer::callbackTock = callbackTock;
        timer = timerBegin(0, (APB_CLK_FREQ / 1000000 / HWTIMER_TICKS_PER_US), true);
        timerAttachInterrupt(timer, hwTimer::callback, true);
        DBGLN("hwTimer Init");
    }
}

void ICACHE_RAM_ATTR hwTimer::stop()
{
    if (timer && running)
    {
        running = false;
        timerAlarmDisable(timer);
        DBGLN("hwTimer stop");
    }
}

void ICACHE_RAM_ATTR hwTimer::resume()
{
    if (timer && !running)
    {
        // The timer must be restarted so that the new timerAlarmWrite() period is set.
        timerRestart(timer);
#if defined(TARGET_TX)
        timerAlarmWrite(timer, HWtimerInterval, true);
#else
        // The STM32 timer fires tock() ASAP after enabling, so mimic that behavior
        // tock() should always be the first event to maintain consistency
        isTick = false;
        // When using EDGE triggered timer, enabling the timer causes an edge so the interrupt
        // is fired immediately, so this emulates the STM32 behaviour
        // Unlike the 8266 timer, the ESP32 timer can be started without delay.
        // It does not interrupt the currently running IsrCallback(), but triggers immediately once it has completed.
        timerAlarmWrite(timer, 0 * HWTIMER_TICKS_PER_US, true);
#endif
        running = true;
        timerAlarmEnable(timer);
        DBGLN("hwTimer resume");
    }
}

void ICACHE_RAM_ATTR hwTimer::updateInterval(uint32_t time)
{
    // timer should not be running when updateInterval() is called
    HWtimerInterval = time * HWTIMER_TICKS_PER_US;
    if (timer)
    {
        DBGLN("hwTimer interval: %d", time);
        timerAlarmWrite(timer, HWtimerInterval, true);
    }
}

void ICACHE_RAM_ATTR hwTimer::phaseShift(int32_t newPhaseShift)
{
    int32_t minVal = -(HWtimerInterval >> 2);
    int32_t maxVal = (HWtimerInterval >> 2);

    // phase shift is in microseconds
    PhaseShift = constrain(newPhaseShift, minVal, maxVal) * HWTIMER_TICKS_PER_US;
}

void ICACHE_RAM_ATTR hwTimer::callback(void)
{
    if (running)
    {
        portENTER_CRITICAL_ISR(&isrMutex);
#if defined(TARGET_TX)
        callbackTock();
#else
        uint32_t NextInterval = (HWtimerInterval >> 1) + FreqOffset;
        if (hwTimer::isTick)
        {
            timerAlarmWrite(timer, NextInterval, true);
            hwTimer::callbackTick();
        }
        else
        {
            NextInterval += PhaseShift;
            timerAlarmWrite(timer, NextInterval, true);
            PhaseShift = 0;
            hwTimer::callbackTock();
        }
        hwTimer::isTick = !hwTimer::isTick;
#endif
        portEXIT_CRITICAL_ISR(&isrMutex);
    }
}

#endif

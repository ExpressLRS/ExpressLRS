
#ifdef PLATFORM_ESP32
#include "ESP32_hwTimer.h"
#include "logging.h"

static hw_timer_t *timer = NULL;
static portMUX_TYPE isrMutex = portMUX_INITIALIZER_UNLOCKED;

void hwTimer::nullCallback(void) {}

#if defined(TARGET_RX)
void (*hwTimer::callbackTick)() = &nullCallback;
#endif
void (*hwTimer::callbackTock)() = &nullCallback;

volatile uint32_t hwTimer::HWtimerInterval = TimerIntervalUSDefault;
volatile bool hwTimer::running = false;

#if defined(TARGET_RX)
volatile bool hwTimer::isTick = false;
volatile int32_t hwTimer::PhaseShift = 0;
volatile int32_t hwTimer::FreqOffset = 0;

#define HWTIMER_TICKS_PER_US 5
#else
#define HWTIMER_TICKS_PER_US 1
#endif

void ICACHE_RAM_ATTR hwTimer::init()
{
    if (!timer)
    {
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
        running = true;

#if defined(TARGET_TX)
        timerAlarmWrite(timer, HWtimerInterval, true);
#else
        // The STM32 timer fires tock() ASAP after enabling, so mimic that behavior
        // tock() should always be the first event to maintain consistency
        isTick = false;
        // When using EDGE triggered timer, enabling the timer causes an edge so the interrupt
        // is fired immediately, so this emulates the STM32 behaviour
        timerAlarmWrite(timer, HWtimerInterval >> 1, true);
#endif
        timerAlarmEnable(timer);
        DBGLN("hwTimer resume");
    }
}

void ICACHE_RAM_ATTR hwTimer::updateInterval(uint32_t time)
{
    HWtimerInterval = time * HWTIMER_TICKS_PER_US;
#if defined(TARGET_TX)
    if (timer)
    {
        DBGLN("hwTimer interval: %d", time);
        timerAlarmWrite(timer, HWtimerInterval, true);
    }
#endif
}

#if defined(TARGET_RX)
void ICACHE_RAM_ATTR hwTimer::resetFreqOffset()
{
    FreqOffset = 0;
}

void ICACHE_RAM_ATTR hwTimer::incFreqOffset()
{
    FreqOffset++;
}

void ICACHE_RAM_ATTR hwTimer::decFreqOffset()
{
    FreqOffset--;
}

void ICACHE_RAM_ATTR hwTimer::phaseShift(int32_t newPhaseShift)
{
    int32_t minVal = -(hwTimer::HWtimerInterval >> 2);
    int32_t maxVal = (hwTimer::HWtimerInterval >> 2);

    // phase shift is in microseconds
    hwTimer::PhaseShift = constrain(newPhaseShift, minVal, maxVal) * HWTIMER_TICKS_PER_US;
}
#endif

void ICACHE_RAM_ATTR hwTimer::callback(void)
{
    if (running)
    {
        portENTER_CRITICAL_ISR(&isrMutex);
#if defined(TARGET_TX)
        callbackTock();
#else
        uint32_t NextInterval = (hwTimer::HWtimerInterval >> 1) + FreqOffset;
        if (hwTimer::isTick)
        {
            timerAlarmWrite(timer, NextInterval, true);
            hwTimer::callbackTick();
        }
        else
        {
            NextInterval += hwTimer::PhaseShift;
            timerAlarmWrite(timer, NextInterval, true);
            hwTimer::PhaseShift = 0;
            hwTimer::callbackTock();
        }
        hwTimer::isTick = !hwTimer::isTick;
#endif
        portEXIT_CRITICAL_ISR(&isrMutex);
    }
}

#endif

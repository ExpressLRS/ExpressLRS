
#ifdef PLATFORM_ESP32
#include "ESP32_hwTimer.h"

static hw_timer_t *timer = NULL;
static portMUX_TYPE isrMutex = portMUX_INITIALIZER_UNLOCKED;

void hwTimer::nullCallback(void) {}
void (*hwTimer::callbackTock)() = &nullCallback;

volatile uint32_t hwTimer::HWtimerInterval = TimerIntervalUSDefault;

volatile bool hwTimer::running = false;

void ICACHE_RAM_ATTR hwTimer::callback(void)
{
    portENTER_CRITICAL_ISR(&isrMutex);
    if (running)
    {
        callbackTock();
    }
    portEXIT_CRITICAL_ISR(&isrMutex);
}

void ICACHE_RAM_ATTR hwTimer::init()
{
    if (!timer)
    {
        timer = timerBegin(0, (APB_CLK_FREQ / 1000000), true); // us timer
        timerAttachInterrupt(timer, &callback, true);
        timerAlarmWrite(timer, HWtimerInterval, true);
        Serial.println("hwTimer Init");
    }
}

void ICACHE_RAM_ATTR hwTimer::resume()
{
    if (timer)
    {
        running = true;
        timerAlarmWrite(timer, HWtimerInterval, true);
        timerAlarmEnable(timer);
        Serial.println("hwTimer resume");
    }
}

void ICACHE_RAM_ATTR hwTimer::stop()
{
    if (timer)
    {
        running = false;
        timerAlarmDisable(timer);
        Serial.println("hwTimer stop");
    }
}

void ICACHE_RAM_ATTR hwTimer::updateInterval(uint32_t time)
{
    HWtimerInterval = time;
    if (timer)
    {
        Serial.print("hwTimer interval: ");
        Serial.println(time);
        timerAlarmWrite(timer, HWtimerInterval, true);
    }
}
#endif

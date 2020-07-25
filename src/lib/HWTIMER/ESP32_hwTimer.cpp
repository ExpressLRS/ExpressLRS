
#ifdef PLATFORM_ESP32
#include "ESP32_hwTimer.h"

static hw_timer_t *timer = NULL;
static portMUX_TYPE isrMutex = portMUX_INITIALIZER_UNLOCKED;

void hwTimer::nullCallback(void){};
void (*hwTimer::callbackTock)() = &nullCallback;

volatile uint32_t hwTimer::HWtimerInterval = TimerIntervalUSDefault;

volatile bool hwTimer::running = false;

void hwTimer::callback(void)
{
    portENTER_CRITICAL(&isrMutex);
    callbackTock();
    portEXIT_CRITICAL(&isrMutex);
}

void hwTimer::init()
{
    if (!timer)
    {
        timer = timerBegin(0, (APB_CLK_FREQ / 1000000), true); // us timer
        timerAttachInterrupt(timer, &callback, true);
        Serial.println("hwTimer Init");
    }
    updateInterval(HWtimerInterval);
    timerAlarmEnable(timer);
}

void ICACHE_RAM_ATTR hwTimer::resume()
{
    if (running)
        return;

    if (!timer)
    {
        init();
        running = true;
        timerStart(timer);
    }
}

void ICACHE_RAM_ATTR hwTimer::stop()
{
    Serial.println("hwTimer stop");
    if (timer)
    {
        timerEnd(timer);
        timer = NULL;
        running = false;
    }
}

void ICACHE_RAM_ATTR hwTimer::updateInterval(uint32_t time)
{
    HWtimerInterval = time;
    if (timer)
    {
        timerAlarmWrite(timer, HWtimerInterval, true);
    }
}
#endif
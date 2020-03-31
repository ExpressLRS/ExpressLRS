#include "HwTimer.h"
#include "LoRa_SX127x.h"
#include <FreeRTOS.h>
#include <esp32-hal-timer.h>

static hw_timer_t *timer = NULL;
static portMUX_TYPE isrMutex = portMUX_INITIALIZER_UNLOCKED;

void ICACHE_RAM_ATTR TimerTask_ISRhandler(void)
{
    portENTER_CRITICAL(&isrMutex);
    HwTimer::callback();
    portEXIT_CRITICAL(&isrMutex);
}

void HwTimer::init()
{
    if (!timer)
    {
        timer = timerBegin(0, (APB_CLK_FREQ / 1000000), true); // us timer
        timerAttachInterrupt(timer, &TimerTask_ISRhandler, true);
        timerAlarmWrite(timer, HwTimer::HWtimerInterval, true);
        timerAlarmEnable(timer);
    }
}

void HwTimer::stop()
{
    /* are these rly needed?? */
    detachInterrupt(SX127xDriver::SX127x_dio0);
    SX127xDriver::ClearIRQFlags();
    if (timer)
    {
        timerEnd(timer);
        timer = NULL;
    }
}

void HwTimer::pause()
{
    stop();
}

void HwTimer::setTime(uint32_t time)
{
    if (timer)
    {
        timerAlarmWrite(timer, time, true);
    }
}

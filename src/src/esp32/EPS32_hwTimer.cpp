#include "HwTimer.h"
#include "targets.h"
#include "LoRa_SX127x.h"
#include <FreeRTOS.h>
#include <esp32-hal-timer.h>
#include "debug.h"

HwTimer TxTimer;

static hw_timer_t *timer = NULL;
static portMUX_TYPE isrMutex = portMUX_INITIALIZER_UNLOCKED;

void ICACHE_RAM_ATTR TimerTask_ISRhandler(void)
{
    portENTER_CRITICAL(&isrMutex);
    TxTimer.callback();
    portEXIT_CRITICAL(&isrMutex);
}

void HwTimer::init()
{
    if (!timer)
    {
        timer = timerBegin(0, (APB_CLK_FREQ / 1000000), true); // us timer
        timerAttachInterrupt(timer, &TimerTask_ISRhandler, true);
    }
    timerStop(timer);
    setTime(HWtimerInterval >> 1);
    timerAlarmEnable(timer);
    timerStart(timer);
}

void HwTimer::start()
{
    if (timer)
        timerStart(timer);
    else
        init();
}

void HwTimer::stop()
{
    /* are these rly needed?? */
    detachInterrupt(GPIO_PIN_DIO0);
    Radio.ClearIRQFlags();

    /*if (timer)
    {
        timerEnd(timer);
        timer = NULL;
    }*/
    if (timer)
        timerStop(timer);
}

void HwTimer::pause()
{
    if (timer)
        timerAlarmDisable(timer);
}

void HwTimer::setTime(uint32_t time)
{
    if (timer)
    {
        timerAlarmWrite(timer, time, true);
    }
}

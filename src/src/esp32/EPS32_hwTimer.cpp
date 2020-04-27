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
    stop(timer);
    setTime(HWtimerInterval);
    timerAlarmEnable(timer);
}

void ICACHE_RAM_ATTR HwTimer::start()
{
    if (!timer)
        init();
    running = true;
    timerStart(timer);
}

void ICACHE_RAM_ATTR HwTimer::stop()
{
    /* are these rly needed?? */
    detachInterrupt(GPIO_PIN_DIO0);
    Radio.ClearIRQFlags();

    running = false;

    if (timer)
        timerStop(timer);
}

void ICACHE_RAM_ATTR HwTimer::pause()
{
    running = false;
    if (timer)
        timerAlarmDisable(timer);
}

void ICACHE_RAM_ATTR HwTimer::reset()
{
    if (timer && running)
        timerWrite(timer, 0);
}

void ICACHE_RAM_ATTR HwTimer::setTime(uint32_t time)
{
    if (timer)
    {
        if (!time)
            time = HWtimerInterval;
        timerAlarmWrite(timer, time, true);
    }
}

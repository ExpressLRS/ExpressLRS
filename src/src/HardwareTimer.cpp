#include "Arduino.h"
#include "HardwareTimer.h"

#ifdef PLATFORM_STM32
#if defined(TIM1)
TIM_TypeDef *Instance = TIM1;
#else
TIM_TypeDef *Instance = TIM2;
#endif
HardwareTimer *MyTim = new HardwareTimer(Instance);
#endif

void Update_IT_callback(HardwareTimer *)
{ // Toggle pin. 10hz toogle --> 5Hz PWM
    digitalWrite(GPIO_PIN_LED_GREEN, !digitalRead(GPIO_PIN_LED_GREEN));
}

uint32_t ICACHE_RAM_ATTR HWtimerGetlastCallbackMicros()
{
    return HWtimerLastCallbackMicros;
}

uint32_t ICACHE_RAM_ATTR HWtimerGetlastCallbackMicros90()
{
    return HWtimerLastCallbackMicros90;
}

uint32_t ICACHE_RAM_ATTR HWtimerGetIntervalMicros()
{
    return HWtimerIntervalUS;
}

void ICACHE_RAM_ATTR HWtimerUpdateInterval(uint32_t _TimerInterval)
{
    Serial.println("HW upd");
#ifdef PLATFORM_STM32
    HWtimerInterval = _TimerInterval;
    MyTim->setOverflow(HWtimerInterval >> 1, MICROSEC_FORMAT);
#else
    HWtimerInterval = _TimerInterval * 5;
    timer1_write(HWtimerInterval >> 1);
#endif
    HWtimerIntervalUS = _TimerInterval;
}

void ICACHE_RAM_ATTR HWtimerPhaseShift(int16_t NewOffset)
{
    int16_t MaxPhaseShift = HWtimerInterval >> 1;

    if (NewOffset > MaxPhaseShift)
    {
        PhaseShift = MaxPhaseShift;
    }
    else
    {
        PhaseShift = NewOffset;
    }

    if (NewOffset < -MaxPhaseShift)
    {
        PhaseShift = -MaxPhaseShift;
    }
    else
    {
        PhaseShift = NewOffset;
    }

#ifdef PLATFORM_STM32
    PhaseShift = PhaseShift;
#else
    PhaseShift = PhaseShift * 5;
#endif
}

#ifdef PLATFORM_STM32
void ICACHE_RAM_ATTR Timer0Callback(HardwareTimer *)
#else
void ICACHE_RAM_ATTR Timer0Callback()
#endif
{

    if (TickTock)
    {
        if (ResetNextLoop)
        {
#ifdef PLATFORM_STM32
            MyTim->setOverflow(HWtimerInterval >> 1, MICROSEC_FORMAT);
#else
            timer1_write(HWtimerInterval >> 1);
#endif
            ResetNextLoop = false;
        }

        if (PhaseShift > 0 || PhaseShift < 0)
        {
#ifdef PLATFORM_STM32
            MyTim->setOverflow((HWtimerInterval + PhaseShift) >> 1, MICROSEC_FORMAT);
#else
            timer1_write((HWtimerInterval + PhaseShift) >> 1);
#endif
            ResetNextLoop = true;
            PhaseShift = 0;
        }

        //uint32_t next = ESP.getCycleCount() + HWtimerInterval * 5;
        //timer1_write(next + PhaseShift); // apply phase shift to next cycle
        //PhaseShift = 0; //then reset the phase shift variable
        HWtimerCallBack();

        HWtimerLastCallbackMicros = micros();
    }
    else
    {
        HWtimerCallBack90();
        HWtimerLastCallbackMicros90 = micros();
    }
    TickTock = !TickTock;
}

void ICACHE_RAM_ATTR InitHarwareTimer()
{
    Serial.println("HWinit");
    noInterrupts();
#ifdef PLATFORM_STM32
    //MyTim->setMode(1, TIMER_OUTPUT_COMPARE);
    MyTim->setOverflow(HWtimerInterval >> 1, MICROSEC_FORMAT);
    MyTim->attachInterrupt(Timer0Callback);
    MyTim->resume();
    interrupts();
#else
    timer1_attachInterrupt(Timer0Callback);
    timer1_enable(TIM_DIV16, TIM_EDGE, TIM_LOOP); //5MHz ticks
    timer1_write(HWtimerInterval);                //120000 us
    interrupts();
#endif
}

void StopHWtimer()
{
    Serial.println("HWstop");
#ifdef PLATFORM_STM32
    //MyTim->detachInterrupt();
    //MyTim->timerHandleDeinit();
    MyTim->pause();
#else
    timer1_detachInterrupt();
#endif
}

void HWtimerSetCallback(void (*CallbackFunc)(void))
{
    Serial.println("setCB");
    HWtimerCallBack = CallbackFunc;
}

void HWtimerSetCallback90(void (*CallbackFunc)(void))
{
    Serial.println("setCB90");
    HWtimerCallBack90 = CallbackFunc;
}
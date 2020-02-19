#include "STM32_hwTimer.h"

void inline hwTimer::nullCallback(void){};

void (*hwTimer::callbackTick)() = &nullCallback; // function is called whenever there is new RC data.
void (*hwTimer::callbackTock)() = &nullCallback; // function is called whenever there is new RC data.

volatile uint32_t hwTimer::HWtimerInterval = TimerIntervalUSDefault;
bool hwTimer::TickTock = false;
volatile int32_t hwTimer::PhaseShift = 0;
bool hwTimer::ResetNextLoop = false;

uint32_t hwTimer::LastCallbackMicrosTick = 0;
uint32_t hwTimer::LastCallbackMicrosTock = 0;

bool hwTimer::NewIntervalReq = false;
volatile uint32_t hwTimer::newHWtimerInterval = 0;

uint8_t hwTimer::TimerDiv = 1;
uint8_t hwTimer::Counter = 0;

uint8_t hwTimer::Prescaler = 0;
bool hwTimer::UpdatePrescaler = false;

HardwareTimer(*hwTimer::MyTim) = new HardwareTimer(TIM1);

void hwTimer::init()
{
    noInterrupts();
    MyTim->attachInterrupt(hwTimer::callback);
    //MyTim->setMode(2, TIMER_OUTPUT_COMPARE);
    MyTim->setOverflow(hwTimer::HWtimerInterval >> 1, MICROSEC_FORMAT);
    //MyTim->setInterruptPriority(0, 0);
    MyTim->resume();
    interrupts();
    Serial.print("Scaler:");
    hwTimer::Prescaler = MyTim->getPrescaleFactor();
    Serial.println(MyTim->getPrescaleFactor());

    Serial.print("TickOverflow: ");
    Serial.println(MyTim->getOverflow());

    MyTim->refresh();
}

void hwTimer::stop()
{
    MyTim->pause();
}

void hwTimer::pause()
{
    MyTim->pause();
}

void hwTimer::setPrescaler(uint32_t newPrescaler)
{
    hwTimer::Prescaler = newPrescaler;
    hwTimer::UpdatePrescaler = true;

    //Serial.print("New Scaler:");
    //Serial.println(MyTim->getPrescaleFactor());
}

void hwTimer::updateInterval(uint32_t Interval)
{
    hwTimer::newHWtimerInterval = Interval;
    NewIntervalReq = true;
    hwTimer::PhaseShift = hwTimer::PhaseShift;
    MyTim->refresh();

    // //Serial.print("==========:");
    // //Serial.println(MyTim->getCount());

    // //Serial.print("==========:");
    // Serial.println(MyTim->getCount());

    // //hwTimer::phaseShiftNoLimit(newTimerInterval >> 1);
    // hwTimer::phaseShiftNoLimit(-1500);

    // hwTimer::HWtimerInterval = newTimerInterval;
    // MyTim->setOverflow(hwTimer::HWtimerInterval >> 1, MICROSEC_FORMAT);
    // //MyTim->refresh();
}

void ICACHE_RAM_ATTR hwTimer::phaseShift(int32_t newPhaseShift)
{
    //Serial.println(newPhaseShift);
    int32_t MaxPhaseShift = hwTimer::HWtimerInterval >> 2;

    if (newPhaseShift > MaxPhaseShift)
    {
        hwTimer::PhaseShift = MaxPhaseShift;
    }
    else
    {
        hwTimer::PhaseShift = newPhaseShift;
    }

    if (newPhaseShift < -MaxPhaseShift)
    {
        hwTimer::PhaseShift = -MaxPhaseShift;
    }
    else
    {
        hwTimer::PhaseShift = newPhaseShift;
    }
}

void ICACHE_RAM_ATTR hwTimer::phaseShiftNoLimit(int32_t newPhaseShift)
{
    hwTimer::PhaseShift = newPhaseShift;
}

void ICACHE_RAM_ATTR hwTimer::callback(HardwareTimer *)
{
    if (hwTimer::TickTock)
    {
        //if (NewIntervalReq)
        //{
        //    NewIntervalReq = false;
        //     hwTimer::HWtimerInterval = newHWtimerInterval;
        // }

        if (hwTimer::UpdatePrescaler)
        {
            hwTimer::UpdatePrescaler = false;
            MyTim->setPrescaleFactor(hwTimer::Prescaler);
        }

        if (hwTimer::ResetNextLoop)
        {
            //MyTim->setOverflow(hwTimer::HWtimerInterval >> 1, MICROSEC_FORMAT);
            MyTim->setOverflow(60000, TICK_FORMAT);
            hwTimer::ResetNextLoop = false;
        }

        //if (hwTimer::PhaseShift > 1 || hwTimer::PhaseShift < -1)
        // {
        //MyTim->setOverflow(((hwTimer::HWtimerInterval + hwTimer::PhaseShift) >> 1), MICROSEC_FORMAT);
        MyTim->setOverflow(60000 + hwTimer::PhaseShift, TICK_FORMAT);

        hwTimer::ResetNextLoop = true;
        hwTimer::PhaseShift = 0;
        //}

        hwTimer::LastCallbackMicrosTick = micros();
        hwTimer::callbackTick();
#ifdef TARGET_R9M_RX
        digitalWrite(PA8, HIGH);
#endif
    }
    else
    {
        hwTimer::LastCallbackMicrosTock = micros();
        hwTimer::callbackTock();
#ifdef TARGET_R9M_RX
        digitalWrite(PA8, LOW);
#endif
    }
    hwTimer::Counter++;
    hwTimer::TickTock = !hwTimer::TickTock;
}
#pragma once

#include <stdio.h>
#include "targets.h"
#include "PIC32_Timer_Library.h"

#define TimerIntervalUSDefault 20000

class hwTimer
{
public:

    static volatile uint32_t HWtimerInterval;
    static volatile bool isTick;
    static volatile int32_t PhaseShift;
    static volatile int32_t FreqOffset;
    static volatile uint32_t PauseDuration;
    static bool running;
    static bool alreadyInit;
    

    static void init(); //-----------------------------------// TX/RX
    static void stop(); //-----------------------------------// TX/RX
    static void pause(uint32_t duration); //-----------------// TX
    static void resume(); //---------------------------------// TX/RX
    static void callback(void);  //--------------------------// TX/RX
    static void updateInterval(uint32_t newTimerInterval);   // TX
    static void resetFreqOffset();  //-----------------------// RX
    static void incFreqOffset();  //-------------------------// RX
    static void decFreqOffset();  //-------------------------// RX
    static void phaseShift(int32_t newPhaseShift);  //-------// RX

    static void inline nullCallback(void);
    static void (*callbackTick)();
    static void (*callbackTock)();
};


// //Forward references to library functions
// void startTimer(uint8_t timerNum, long microseconds);
// void stopTimer(uint8_t timerNum);
// void setTimerPeriod(uint8_t timerNum, long microseconds);
// void timerReset(uint8_t timerNum);
// void attachTimerInterrupt(uint8_t timerNum, void (*userFunc)(void));
// void detachTimerInterrupt(uint8_t timerNum);
// void disableTimerInterrupt(uint8_t timerNum);
// void enableTimerInterrupt(uint8_t timerNum);

// void startPWM(uint8_t timerNum, uint8_t OCnum, uint8_t dutycycle);
// void stopPWM(uint8_t OCnum);
// void setDutyCycle(uint8_t OCnum, float dutycycle);
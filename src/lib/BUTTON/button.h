#pragma once

#include "targets.h"

class button
{
private:
    static int buttonPin;
    static bool activeHigh;

    static uint32_t buttonLastPressed;
    static uint32_t buttonLastPressedLong;
    static uint32_t buttonLastPressedShort;

    static bool buttonPrevState;  //active high, therefore true as default.
    static bool buttonIsDown;     //is the button currently being held down?
    static bool buttonIsDownLong; //is the button currently being held down?

    static uint32_t debounceDelay;     //how long the switch must change state to be considered
    static uint32_t longPressDelay;    //how long the switch must hold state to be considered a long press
    static uint32_t longPressInterval; //how long the switch must hold long state to be reapeated.
    static uint32_t triplePressInterval; //how long the switch short press time to be recounted
    
    static uint8_t shortPressTime;    //sustain short press time

    static void sampleButton();

public:
    static void init(int Pin, bool activeHigh = true);
    static void handle();

    static void inline nullCallback(void);

    static void (*buttonShortPress)();
    static void (*buttonLongPress)();
    static void (*buttonTriplePress)();
};
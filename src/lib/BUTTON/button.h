#pragma once

#include <Arduino.h>
#include "../../src/targets.h"

class Button
{
private:
    int buttonPin;
    int activeHigh;

    uint32_t buttonLastPressed;
    uint32_t buttonLastPressedLong;

    bool buttonPrevState;  //active high, therefore true as default.
    bool buttonIsDown;     //is the button currently being held down?
    bool buttonIsDownLong; //is the button currently being held down?

    uint32_t debounceDelay;     //how long the switch must change state to be considered
    uint32_t longPressDelay;    //how long the switch must hold state to be considered a long press
    uint32_t longPressInterval; //how long the switch must hold long state to be reapeated.

    void sampleButton();

public:
    Button();
    void init(int Pin, bool activeHigh = true);
    void handle();

    static void inline nullCallback(void);

    void (*buttonShortPress)();
    void (*buttonLongPress)();
};

#pragma once

#include <stdint.h>

class Button
{
private:
    int buttonPin;
    int p_inverted;

    uint32_t p_nextSamplingTime;
    uint32_t buttonLastPressed;
    uint32_t buttonLastPressedLong;

    bool buttonPrevState;  // active high, therefore true as default.
    bool buttonIsDown;     // is the button currently being held down?
    bool buttonIsDownLong; // is the button currently being held down?

    uint32_t debounceDelay;     // how long the switch must change state to be considered
    uint32_t shortPressDelay;   // how long the switch must hold state to be considered a short press
    uint32_t longPressDelay;    // how long the switch must hold state to be considered a long press
    uint32_t longPressInterval; // how long the switch must hold long state to be reapeated.

    void sampleButton(uint32_t now);

public:
    Button(int debounce = -1);
    void set_press_delay_short(uint32_t delay);
    void set_press_delay_long(uint32_t delay, uint32_t interval = 500);
    void init(int Pin, bool inverted = true, int debounce = -1);
    void handle();
    void handle(uint32_t now);

    /* Button event callback, time is how long button is/was pressed */
    void (*buttonShortPress)(uint32_t time);
    void (*buttonLongPress)(uint32_t time);
};

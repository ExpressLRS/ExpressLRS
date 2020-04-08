#include "button.h"
#include <Arduino.h>

static void nullCallback(uint32_t){};

Button::Button(int debounce)
{
    if (debounce < 0)
        debounce = 80;

    buttonShortPress = &nullCallback;
    buttonLongPress = &nullCallback;

    p_nextSamplingTime = 0;

    buttonLastPressed = 0;
    buttonLastPressedLong = 0;

    buttonPrevState = false;
    buttonIsDown = false;
    buttonIsDownLong = false;

    debounceDelay = debounce;
    shortPressDelay = 100;
    longPressDelay = 500;
    longPressInterval = 500;

    buttonPin = -1;
    p_inverted = 0;
}

void Button::init(int Pin, bool inverted, int debounce)
{
    pinMode(Pin, (inverted ? INPUT_PULLUP : INPUT));
    buttonPin = Pin;

    buttonPrevState = false;
    buttonIsDown = false;
    buttonIsDownLong = false;

    p_inverted = inverted;
    if (0 <= debounce)
        debounceDelay = debounce;
}

void Button::handle()
{
    if (buttonPin < 0)
        return;
    sampleButton();
}

void Button::sampleButton()
{
    uint32_t now = millis();
    uint32_t interval = 10;
    if (now >= p_nextSamplingTime)
    {
        uint32_t press_time = now - buttonLastPressed;
        const bool currState = !!digitalRead(buttonPin) ^ p_inverted;

        if (currState == false && buttonPrevState == true)
        { //button release
            if (buttonIsDown && (!buttonIsDownLong))
            {
                // Check that button was pressed long enough
                if (press_time > shortPressDelay)
                {
                    buttonShortPress(press_time);
                }
            }
            buttonIsDown = false;
            buttonIsDownLong = false;
        }
        else if (currState == true && buttonPrevState == false)
        { //button pressed
            buttonLastPressed = now;
            interval = debounceDelay;
        }
        else if (currState == true && buttonPrevState == true)
        { // button hold
            buttonIsDown = true;

            if ((press_time > longPressDelay) &&
                ((now - buttonLastPressedLong) > longPressInterval))
            {
                buttonLongPress(press_time);

                buttonLastPressedLong = now;
                buttonIsDownLong = true;
            }
        }

        buttonPrevState = currState;
        p_nextSamplingTime = now + interval;
    }
}

#include "button.h"
#include "../../src/debug.h"

void inline Button::nullCallback(void){};

Button::Button()
{
    buttonShortPress = &nullCallback; // callbacks
    buttonLongPress = &nullCallback;  // callbacks

    buttonLastPressed = 0;
    buttonLastPressedLong = 0;

    buttonPrevState = true;   //active high, therefore true as default.
    buttonIsDown = false;     //is the button currently being held down?
    buttonIsDownLong = false; //is the button currently being held down?

    debounceDelay = 80;      //how long the switch must change state to be considered
    longPressDelay = 500;    //how long the switch must hold state to be considered a long press
    longPressInterval = 500; //how long the switch must hold long state to be reapeated.

    buttonPin = -1;
    activeHigh = 0;
}

void Button::init(int Pin, bool high)
{
    pinMode(Pin, (high ? INPUT_PULLUP : INPUT));
    buttonPin = Pin;
    activeHigh = high;
}

void Button::handle()
{
    sampleButton();
}

void Button::sampleButton()
{
    if (buttonPin < 0)
        return;

    bool currState = !!digitalRead(buttonPin) ^ activeHigh;
    uint32_t now = millis();

    if (currState == false && buttonPrevState == true)
    { //button release
        if (buttonIsDown && (!buttonIsDownLong))
        {
            DEBUG_PRINTLN("button short pressed");
            buttonShortPress();
        }
        buttonIsDown = false; //button has been released at some point
        buttonIsDownLong = false;
    }
    else if (currState == true && buttonPrevState == false)
    { //button pressed
        buttonLastPressed = now;
    }

    if (currState == true && buttonPrevState == true && (now - buttonLastPressed) > debounceDelay)
    {
        buttonIsDown = true;
    }

    if (buttonIsDown &&
        ((now - buttonLastPressed) > longPressDelay) &&
        ((now - buttonLastPressedLong) > longPressInterval))
    {
        if (buttonIsDown)
        {
            DEBUG_PRINTLN("button pressed long");
            buttonLastPressedLong = now;
            buttonIsDownLong = true;
            buttonLongPress();
        }
    }

    buttonPrevState = currState;
}

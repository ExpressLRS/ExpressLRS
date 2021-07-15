#include "button.h"

void inline button::nullCallback(void) {}
void (*button::buttonShortPress)() = &nullCallback; // callbacks
void (*button::buttonLongPress)() = &nullCallback;  // callbacks
void (*button::buttonTriplePress)() = &nullCallback;  // callbacks

uint32_t button::buttonLastPressed = 0;
uint32_t button::buttonLastPressedLong = 0;
uint32_t button::buttonLastPressedShort = 0;

bool button::buttonPrevState = true;   //active high, therefore true as default.
bool button::buttonIsDown = false;     //is the button currently being held down?
bool button::buttonIsDownLong = false; //is the button currently being held down?

uint32_t button::debounceDelay = 30;      //how long the switch must change state to be considered
uint32_t button::longPressDelay = 500;    //how long the switch must hold state to be considered a long press
uint32_t button::longPressInterval = 500; //how long the switch must hold long state to be reapeated.
uint32_t button::triplePressInterval = 500; //how long the switch short press time to be recounted

uint8_t button::shortPressTime = 0;

int button::buttonPin = -1;
bool button::activeHigh = true;

void button::init(int Pin, bool ActiveHigh)
{
    if (activeHigh)
    {
        pinMode(Pin, INPUT_PULLUP);
    }
    else
    {
        pinMode(Pin, INPUT);
    }

    buttonPin = Pin;
    activeHigh = ActiveHigh;
}

void button::handle()
{
    sampleButton();
}

void button::sampleButton()
{
    bool currState = digitalRead(buttonPin);
    uint32_t now = millis();
    if (activeHigh)
    {
        if (currState == true and buttonPrevState == false)
        { //button release
            if (buttonIsDown and (!buttonIsDownLong))
            {
                Serial.println("button short pressed");
                buttonShortPress();
                buttonLastPressedShort = now;
                shortPressTime++;
            }
            buttonIsDown = false; //button has been released at some point
            buttonIsDownLong = false;
        }
    }
    else
    {
        if (currState == false and buttonPrevState == true)
        { //button release
            if (buttonIsDown and (!buttonIsDownLong))
            {
                Serial.println("button short pressed");
                buttonShortPress();
                buttonLastPressedShort = now;
                shortPressTime++;
            }
            buttonIsDown = false; //button has been released at some point
            buttonIsDownLong = false;
        }
    }

    if (activeHigh)
    {
        if (currState == false and buttonPrevState == true)
        { //falling edge
            buttonLastPressed = millis();
        }
    }
    else
    {
        if (currState == true and buttonPrevState == false)
        { //falling edge
            buttonLastPressed = millis();
        }
    }

    if (activeHigh)
    {
        if (currState == false and buttonPrevState == false and (millis() - buttonLastPressed) > debounceDelay)
        {
            buttonIsDown = true;
        }
    }
    else
    {
        if (currState == true and buttonPrevState == true and (millis() - buttonLastPressed) > debounceDelay)
        {
            buttonIsDown = true;
        }
    }

    if (buttonIsDown and ((millis() - buttonLastPressed) > longPressDelay) and (millis() - buttonLastPressedLong > longPressInterval))
    {
        if (buttonIsDown)
        {
            Serial.println("button pressed long");
            buttonLastPressedLong = now;
            buttonIsDownLong = true;
            buttonLongPress();
        }
    }

    if(shortPressTime == 3)
    {
        Serial.println("button triple pressed");
        buttonTriplePress();
        shortPressTime = 0;    
    }
    if ((millis() - buttonLastPressedShort) > triplePressInterval)
    {
        shortPressTime = 0;
    }

    buttonPrevState = currState;
}
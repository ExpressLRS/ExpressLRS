#ifndef ClickButton_H
#define ClickButton_H

#include <Arduino.h>

class ClickButton
{
public:
    ClickButton(uint8_t const buttonPin);
    ClickButton(uint8_t const buttonPin, boolean const activeState);
    ClickButton(uint8_t const buttonPin,
                boolean const activeState,
                unsigned long const _debounceTime,
                unsigned long const _multiclickTime,
                unsigned long const _longClickTime);

    void update(void);
    void update(unsigned long const &millis);
    boolean getActiveState(void) const
    {
        return _activeHigh;
    }
    void reset(void)
    {
        clicks = 0;
        lastClickLong = 0;
        _clickCount = 0;
    }
    void increaseClicks(uint8_t const num = 1)
    {
        _clickCount += num;
    }

    int8_t clicks;         // button click counts to return
    uint8_t lastClickLong; // Flag if last click was long...
    unsigned long debounceTime;
    unsigned long multiclickTime;
    unsigned long longClickTime;

private:
    unsigned long _lastBounceTime; // the last time the button input pin was toggled, due to noise or a press
    uint8_t _pin;                  // Arduino pin connected to the button
    boolean _activeHigh;           // Type of button: Active-low = 0 or active-high = 1
    uint8_t _clickCount;           // Number of button clicks within multiclickTime milliseconds
    uint8_t _state;
};

#endif

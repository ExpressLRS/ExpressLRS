#include "ClickButton.h"

#define DEBOUNCED_STATE 1 // b0001 = 1 << 0 = 1
#define UNSTABLE_STATE 2  // b0010 = 1 << 2 = 2
#define STATE_CHANGED 8   // b1000 = 1 << 3 = 8

// TODO: Optimize millis usage!!

ClickButton::ClickButton(uint8_t const buttonPin)
{
    _pin = buttonPin;
    _activeHigh = false; // Assume active-low button
    _clickCount = 0;
    _lastBounceTime = millis();
    _state = 0;

    clicks = 0;
    debounceTime = 50;    // Debounce timer in ms
    multiclickTime = 500; // Time limit for multi clicks
    longClickTime = 800;  // time until long clicks register

    pinMode(_pin, INPUT_PULLUP); // Use internal pullup resistor!
    if (digitalRead(_pin))
    {
        _state = DEBOUNCED_STATE | UNSTABLE_STATE;
    }
}

ClickButton::ClickButton(uint8_t const buttonPin, boolean const activeType)
{
    _pin = buttonPin;
    _activeHigh = activeType;
    _clickCount = 0;
    _lastBounceTime = millis();
    _state = 0;

    clicks = 0;
    debounceTime = 50;    // Debounce timer in ms
    multiclickTime = 500; // Time limit for multi clicks
    longClickTime = 800;  // time until long clicks register

    // Turn on internal pullup resistor if applicable
    if (_activeHigh == false)
    {
        pinMode(_pin, INPUT_PULLUP);
    }
    else
    {
        pinMode(_pin, INPUT);
    }
    if (digitalRead(_pin))
    {
        _state = DEBOUNCED_STATE | UNSTABLE_STATE;
    }
}

ClickButton::ClickButton(uint8_t const buttonPin,
                         boolean const activeType,
                         unsigned long const _debounceTime,
                         unsigned long const _multiclickTime,
                         unsigned long const _longClickTime)
{
    _pin = buttonPin;
    _activeHigh = activeType;
    _clickCount = 0;
    _lastBounceTime = millis();
    _state = 0;

    clicks = 0;
    debounceTime = _debounceTime;     // Debounce timer in ms
    multiclickTime = _multiclickTime; // Time limit for multi clicks
    longClickTime = _longClickTime;   // time until long clicks register

    // Turn on internal pullup resistor if applicable
    if (_activeHigh == false)
    {
        pinMode(_pin, INPUT_PULLUP);
    }
    else
    {
        pinMode(_pin, INPUT);
    }
    if (digitalRead(_pin))
    {
        _state = DEBOUNCED_STATE | UNSTABLE_STATE;
    }
}

void ClickButton::update(unsigned long const &now)
{
    (void)now;
    update();
}

void ClickButton::update(void)
{
    // Read the state of the switch in a temporary variable.
    bool _btnState = false;
    // current appearant button state
    if (_activeHigh)
    {
        _btnState = (digitalRead(_pin) == HIGH ? true : false);
    }
    else
    {
        _btnState = (digitalRead(_pin) == LOW ? true : false);
    }

    _state &= ~STATE_CHANGED;

    // If the reading is different from last reading, reset the debounce counter
    if (_btnState != (bool)(_state & UNSTABLE_STATE))
    {
        _lastBounceTime = millis();
        _state ^= UNSTABLE_STATE;
        return;
    }
    else
    {
        if (debounceTime <= (millis() - _lastBounceTime))
        {
            // We have passed the threshold time, so the input is now stable
            // If it is different from last state, set the STATE_CHANGED flag
            if ((bool)(_state & DEBOUNCED_STATE) != _btnState)
            {
                _lastBounceTime = millis();
                _state ^= DEBOUNCED_STATE;
                _state |= STATE_CHANGED;
            }
        }
        else
        {
            return;
        }
    }

    _btnState = ((_state & DEBOUNCED_STATE) != 0);

    unsigned long const timeDiff = (millis() - _lastBounceTime);
    // debounce the button (Check if a stable, changed state has occured)

    int8_t const __lastClicks = clicks;
    int8_t const __lastClickLong = lastClickLong;
    clicks = 0;
    lastClickLong = 0;

    if ((_state & STATE_CHANGED) && _btnState == false)
    {
        // When button state is changed from press -> realeased
        _clickCount++;
    }
    else if (_btnState == true && _clickCount == 0 && 0 < longClickTime)
    {
        /* First press kept pressed for long enough --> long press */
        if (longClickTime <= timeDiff)
        {
            clicks = -1 * (timeDiff / longClickTime);
        }
    }

    if (_clickCount)
    {
        if (__lastClicks < 0 ||
            (_btnState == false && 0 < _clickCount && 0 < __lastClickLong))
        {
            /* Last one was long press and button is realeased... */
            clicks = 0;
            lastClickLong = 0;
            _clickCount = 0;
        }
        else if (_btnState == true && 0 < _clickCount && 0 < longClickTime)
        {
            // Long press after normal clicks...
            if (longClickTime <= timeDiff)
            {
                lastClickLong = (timeDiff / longClickTime);
                clicks = _clickCount;
            }
        }
        else if (_btnState == false && 0 < multiclickTime)
        {
            if (multiclickTime <= timeDiff)
            {
                // If the button released state is stable, report nr of clicks and start new cycle
                clicks = _clickCount;
                lastClickLong = 0;
                _clickCount = 0;
            }
        }
    }
}

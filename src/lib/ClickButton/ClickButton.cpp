#include "ClickButton.h"
#include <Arduino.h>

#define STATE_LAST      1  // b0001 = 1 << 0 = 1
#define DEBOUNCED_STATE 2  // b0010 = 1 << 1 = 2
#define UNSTABLE_STATE  4  // b0100 = 1 << 2 = 4
#define STATE_CHANGED   8  // b1000 = 1 << 3 = 8

ClickButton::ClickButton(uint8_t const buttonPin,
                         uint8_t const activeType,
                         uint32_t const _debounceTime,
                         uint32_t const _multiclickTime,
                         uint32_t const _longClickTime)
{
    _pin            = buttonPin;
    _activeState    = activeType ? HIGH : LOW;
    _clickCount     = 0;
    _lastBounceTime = 0; //millis()

    clicks          = 0;
    debounceTime    = _debounceTime;            // Debounce timer in ms
    multiclickTime  = _multiclickTime;          // Time limit for multi clicks
    longClickTime   = _longClickTime;           // time until long clicks register

    pinMode(_pin, (_activeState == HIGH) ? INPUT : INPUT_PULLUP);
    _state = _btnStateLast = 0; //digitalRead(_pin);
}


void ClickButton::update(void)
{
    uint32_t now = millis();
    update(now);
}


void ClickButton::update(uint32_t const & time_ms)
{
    uint32_t const __timeDiff = (time_ms - _lastBounceTime);

    if ((_state & STATE_CHANGED) && debounceTime > __timeDiff) {
        return;
    }

    // Read the state of the switch
    uint8_t _btnState = (digitalRead(_pin) == _activeState);

    if (_btnState != _btnStateLast) {
        // If the reading is different from last reading, reset the debounce counter
        _lastBounceTime = time_ms;
        _btnStateLast   = _btnState;
        _state          = STATE_CHANGED;
        return;
    }

    int8_t   const __lastClicks    = clicks;
    uint8_t  const __lastClickLong = lastClickLong;
    // reset clicks
    clicks        = 0;
    lastClickLong = 0;

    if ((_state & STATE_CHANGED) && _btnState == false) {
        // When button state is changed from press -> realeased
        if (__lastClicks < 0) {
            // reset after long press
            reset();
        } else {
            _clickCount++;
        }

    } else if (_btnState == true && _clickCount == 0 && 0 < longClickTime) {
        /* First press kept pressed for long enough --> long press */
        if (longClickTime <= __timeDiff) {
            clicks = -1 * (__timeDiff / longClickTime);
        }
    }

    if (_clickCount) {
        if (_btnState == true && 0 < longClickTime) {
            // Long press after normal clicks...
            if (longClickTime <= __timeDiff) {
                lastClickLong = (__timeDiff / longClickTime);
                clicks        = _clickCount;
            }
        } else if (_btnState == false) {
            if (0 < __lastClickLong) {
                /* Last one was long press and button is realeased... */
                clicks          = 0;
                lastClickLong   = 0;
                _clickCount     = 0;
            } else if (0 < multiclickTime && multiclickTime <= __timeDiff) {
                // If the button released state is stable, report nr of clicks and start new cycle
                clicks          = _clickCount;
                lastClickLong   = 0;
                _clickCount     = 0;
            }
        }
    }
    _state = 0;
}

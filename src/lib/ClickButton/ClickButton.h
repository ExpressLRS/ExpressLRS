#ifndef ClickButton_H
#define ClickButton_H

#include <stdint.h>

class ClickButton {
 public:
    ClickButton(uint8_t const buttonPin,
                uint8_t const activeState = false,
                uint32_t const _debounceTime = 50,
                uint32_t const _multiclickTime = 500,
                uint32_t const _longClickTime = 800);

    void update(void);
    void update(uint32_t const & millis);
    uint8_t getActiveState(void) const {
        return _activeState;
    }
    void reset(void) {
        clicks          = 0;
        lastClickLong   = 0;
        _clickCount     = 0;
    }
    void increaseClicks(uint8_t const num = 1) {
        _clickCount += num;
    }

    int8_t   clicks;           // button click counts to return
    uint8_t  lastClickLong;    // Flag if last click was long...
    uint32_t debounceTime;
    uint32_t multiclickTime;
    uint32_t longClickTime;

  private:
    uint32_t _lastBounceTime;  // the last time the button input pin was toggled, due to noise or a press
    uint8_t  _pin;             // Arduino pin connected to the button
    uint8_t  _activeState;     // Type of button: Active-low = 0 or active-high = 1
    uint8_t  _clickCount;      // Number of button clicks within multiclickTime milliseconds
    uint8_t  _state;
    uint8_t  _btnStateLast = 0;
};

#endif

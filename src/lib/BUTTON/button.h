#pragma once

#include <functional>

template <uint8_t PIN, bool IDLELOW>
class Button
{
private:
    // Constants
    static constexpr uint32_t MS_DEBOUNCE = 25;       // how long the switch must change state to be considered
    static constexpr uint32_t MS_LONG = 500;          // duration held to be considered a long press (repeats)
    static constexpr uint32_t MS_MULTI_TIMEOUT = 500; // duration without a press before the short count is reset

    static constexpr unsigned STATE_IDLE = 0b111;
    static constexpr unsigned STATE_FALL = 0b100;
    static constexpr unsigned STATE_RISE = 0b011;
    static constexpr unsigned STATE_HELD = 0b000;

    // State
    uint32_t _lastCheck;  // millis of last pin read
    uint32_t _lastFallingEdge; // millis of last debounced falling edge
    uint8_t _state; // pin history
    bool _isLongPress; // true if last press was a long
    uint8_t _pressCount; // number of short presses before timeout
public:
    // Callbacks
    std::function<void ()>OnShortPress;
    std::function<void ()>OnLongPress;
    // Properties
    uint8_t getCount() const { return _pressCount; }

    Button() :
        _lastCheck(0), _lastFallingEdge(0), _state(STATE_IDLE),
        _isLongPress(false), _pressCount(0)
    {
        pinMode(PIN, IDLELOW ? INPUT : INPUT_PULLUP);
    }

    // Call this in loop()
    void update()
    {
        const uint32_t now = millis();
        if (now - _lastCheck < MS_DEBOUNCE)
            return;
        _lastCheck = now;

        // Reset press count if it has been too long since last rising edge
        if (now - _lastFallingEdge > MS_MULTI_TIMEOUT)
            _pressCount = 0;

        _state = (_state << 1) & 0b110;
        _state |= digitalRead(PIN) ^ IDLELOW;

        // If rising edge (release)
        if (_state == STATE_RISE)
        {
            if (!_isLongPress)
            {
                #if defined(DEBUG_UART)
                DEBUG_UART.println("Button short");
                #endif
                ++_pressCount;
                if (OnShortPress)
                    OnShortPress();
            }
            _isLongPress = false;
        }
        // Two low in a row
        else if (_state == STATE_FALL)
        {
            _lastFallingEdge = now;
        }
        // Three or more low in a row
        else if (_state == STATE_HELD)
        {
            if (now - _lastFallingEdge > MS_LONG)
            {
                #if defined(DEBUG_UART)
                DEBUG_UART.println("Button long");
                #endif
                _isLongPress = true;
                if (OnLongPress)
                    OnLongPress();
                // Reset state so long can fire again
                _state = STATE_IDLE;
            }
        }
    }
};

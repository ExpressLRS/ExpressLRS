#if defined(PLATFORM_ESP8266)
#pragma once

#include <Arduino.h>

class ServoMgr_8266
{
public:
    ServoMgr_8266(uint32_t interval = 20000U)
        : _activePins(0), _refreshInterval(interval)
        {}

    void init(uint8_t pin);
    uint32_t getRefreshInterval() const { return _refreshInterval; }
    void setRefreshInterval(uint32_t intervalUs);
    bool isActive(uint8_t pin) const { return _activePins & (1 << pin); }
    bool isAnyActive() const { return _activePins; }
    void writeMicroseconds(uint8_t pin, uint16_t value);

private:
    uint32_t _activePins;
    uint32_t _refreshInterval;
};

#endif
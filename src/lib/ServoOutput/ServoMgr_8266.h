#if defined(PLATFORM_ESP8266)
#pragma once

#include <Arduino.h>

class ServoMgr_8266
{
public:
    ServoMgr_8266(const uint8_t * const pins, const uint8_t outputCnt, uint32_t defaultInterval = 20000U);

    // Initialize the pins for output
    void initialize();
    // Start/Update PWM
    void writeMicroseconds(uint8_t ch, uint16_t valueUs);
    // Stop PWM
    void stopPwm(uint8_t ch);

    uint16_t getRefreshInterval(uint8_t ch) const { return _refreshInterval[ch]; }
    void setRefreshInterval(uint8_t ch, uint16_t intervalUs);
    bool isActive(uint8_t ch) const { return _activeChannels & (1 << ch); }
    bool isAnyActive() const { return _activeChannels; }
    uint8_t getOutputCnt() const { return _outputCnt; }

private:
    const uint8_t * const _pins;
    const uint8_t _outputCnt;
    uint16_t *_refreshInterval;
    uint32_t _activeChannels;
};

#endif
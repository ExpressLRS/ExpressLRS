#if defined(PLATFORM_ESP32) || defined(PLATFORM_ESP8266)
#pragma once

#include <Arduino.h>
#if defined(PLATFORM_ESP32)
struct timerConfig
{
    uint32_t freq;
    uint8_t ch1;
    uint8_t ch2;

    timerConfig()
        : freq(0), ch1(255), ch2(255){};
};

#endif

class ServoMgr
{
public:
    ServoMgr(const uint8_t *const pins, const uint8_t outputCnt, uint32_t defaultInterval = 20000U);
    ~ServoMgr() { delete[] _refreshInterval; }

    // Initialize the pins for output
    void initialize();
    // Start/Update PWM by pulse width
    void writeMicroseconds(uint8_t ch, uint16_t valueUs);
    // Start/Update PWM by duty
    void writeDuty(uint8_t ch, uint16_t duty);
    // Stop PWM
    void stopPwm(uint8_t ch);
    // Stop any active PWM channels (and set LOW)
    void stopAllPwm();
    // Set a pin high/low (will stopPwm first if active)
    void writeDigital(uint8_t ch, bool value);

    inline uint16_t getRefreshInterval(uint8_t ch) const { return _refreshInterval[ch]; }
    void setRefreshInterval(uint8_t ch, uint16_t intervalUs);
    inline bool isPwmActive(uint8_t ch) const { return _activePwmChannels & (1 << ch); }
    inline bool isAnyPwmActive() const { return _activePwmChannels; }
    inline uint8_t getOutputCnt() const { return _outputCnt; }

    const uint8_t PIN_DISCONNECTED = 0xff;

private:
#if defined(PLATFORM_ESP32)
    timerConfig _timerConfigs[8];
    uint8_t _chnMap[16];
    uint8_t allocateLedcChn(uint8_t ch, uint16_t intervalUs, uint8_t pin);
#endif
    const uint8_t *const _pins;
    const uint8_t _outputCnt;
    uint16_t *_refreshInterval;
    uint32_t _activePwmChannels;
    uint8_t *_resolution_bits;
};

#endif
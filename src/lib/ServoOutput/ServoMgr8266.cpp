#if defined(PLATFORM_ESP8266)

#include "ServoMgr.h"
#include "logging.h"
#include "waveform_8266.h"
#include <math.h>

ServoMgr::ServoMgr(const uint8_t *const pins, const uint8_t outputCnt, uint32_t defaultInterval)
    : _pins(pins), _outputCnt(outputCnt), _refreshInterval(new uint16_t[outputCnt]), _activePwmChannels(0), _resolution_bits(new uint8_t[outputCnt])
{
    for (uint8_t ch = 0; ch < _outputCnt; ++ch)
    {
        _refreshInterval[ch] = defaultInterval;
    }
}

void ServoMgr::initialize()
{
    for (uint8_t ch = 0; ch < _outputCnt; ++ch)
    {
        const uint8_t pin = _pins[ch];
        if (pin != PIN_DISCONNECTED)
        {
            pinMode(pin, OUTPUT);
            digitalWrite(pin, LOW);
        }
    }
}

void ServoMgr::writeMicroseconds(uint8_t ch, uint16_t valueUs)
{
    const uint8_t pin = _pins[ch];
    if (pin == PIN_DISCONNECTED)
    {
        return;
    }
    _activePwmChannels |= (1 << ch);
    startWaveform8266(pin, valueUs, _refreshInterval[ch] - valueUs);
}

void ServoMgr::writeDuty(uint8_t ch, uint16_t duty)
{
    const uint8_t pin = _pins[ch];
    if (pin == PIN_DISCONNECTED)
    {
        return;
    }
    _activePwmChannels |= (1 << ch);
    uint16_t high = map(duty, 0, 1000, 0, _refreshInterval[ch]);
    startWaveform8266(pin, high, _refreshInterval[ch] - high);
}

void ServoMgr::setRefreshInterval(uint8_t ch, uint16_t intervalUs)
{
    if (intervalUs != 0)
    {
        _refreshInterval[ch] = intervalUs;
    }
}

void ServoMgr::stopPwm(uint8_t ch)
{
    const uint8_t pin = _pins[ch];
    if (pin == PIN_DISCONNECTED)
    {
        return;
    }
    _activePwmChannels &= ~(1 << ch);
    stopWaveform8266(pin);
    digitalWrite(pin, LOW);
}

void ServoMgr::stopAllPwm()
{
    for (uint8_t ch = 0; ch < _outputCnt; ++ch)
    {
        stopPwm(ch);
    }
    _activePwmChannels = 0;
}

void ServoMgr::writeDigital(uint8_t ch, bool value)
{
    const uint8_t pin = _pins[ch];
    if (pin == PIN_DISCONNECTED)
    {
        return;
    }
    if (isPwmActive(ch))
    {
        stopPwm(ch);
        // Wait for the last edge, which is at most 1 cycle from now
        delay((_refreshInterval[ch] / 1000U) + 1);
    }
    digitalWrite(pin, value);
}

#endif
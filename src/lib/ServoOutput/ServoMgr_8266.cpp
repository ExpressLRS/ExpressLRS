#if defined(PLATFORM_ESP8266)

#include <ServoMgr_8266.h>
#include <waveform_8266.h>

void ServoMgr_8266::init(uint8_t pin)
{
    pinMode(pin, OUTPUT);
    digitalWrite(pin, LOW);
}

void ServoMgr_8266::writeMicroseconds(uint8_t pin, uint16_t valueUs)
{
    _activePins |= (1 << pin);
    startWaveform8266(pin, valueUs, _refreshInterval - valueUs);
}

void ServoMgr_8266::setRefreshInterval(uint32_t intervalUs)
{
    _refreshInterval = intervalUs;
}

#endif
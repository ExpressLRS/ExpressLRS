#if defined(PLATFORM_ESP8266) || defined(PLATFORM_ESP32)

#include <ServoMgr.h>
#include <waveform_8266.h>
#include <math.h>
#include "logging.h"

ServoMgr::ServoMgr(const uint8_t * const pins, const uint8_t outputCnt, uint32_t defaultInterval)
  : _pins(pins), _outputCnt(outputCnt), _refreshInterval(new uint16_t[outputCnt]), _activePwmChannels(0), _resolution_bits(new uint8_t[outputCnt])
{
    for (uint8_t ch=0; ch<_outputCnt; ++ch)
    {
        _refreshInterval[ch] = defaultInterval;
    }
}

#if defined(PLATFORM_ESP32)
uint8_t  ServoMgr::allocateLedcChn(uint8_t ch, uint16_t intervalUs,uint8_t pin){
    uint32_t  target_freq = 1000000U / intervalUs;
    uint8_t ledc_chn = 255;
    _resolution_bits[ch] = (uint16_t) (log2f(80000000.0f / target_freq) );  // no matter high speed timer or low speed timer, the clk src can be 80Mhz
    if(_resolution_bits[ch]>16){
        _resolution_bits[ch] = 16;
    }

    //for(int i=0;i<8;++i){
    for(int i=7;i>=0;--i){
        if(_timerConfigs[i].freq == 0){
            _timerConfigs[i].freq = target_freq;
            _timerConfigs[i].ch1 = ch;
            ledc_chn = i * 2;

            break;
        }else if(_timerConfigs[i].freq == target_freq && _timerConfigs[i].ch2 == 255){
            _timerConfigs[i].ch2 = ch;
            ledc_chn = i * 2 + 1;
            break;
        }
    }
    DBGLN("allocate ch%d in ledc_ch%d,bits:%d",ch,ledc_chn,_resolution_bits[ch]);
    _chnMap[ch]=ledc_chn;
    ledcSetup(ledc_chn, target_freq, _resolution_bits[ch]);
    ledcAttachPin(pin, ledc_chn);

    return ledc_chn; 
}
#endif

void ServoMgr::initialize()
{
    for (uint8_t ch=0; ch<_outputCnt; ++ch)
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
        return;
    _activePwmChannels |= (1 << pin);
#if defined(PLATFORM_ESP32)
    ledcWrite(_chnMap[ch], map(valueUs, 0, _refreshInterval[ch], 0, (1 << _resolution_bits[ch]) - 1));
#else
    startWaveform8266(pin, valueUs, _refreshInterval[ch] - valueUs);
#endif
}

void ServoMgr::writeDuty(uint8_t ch, uint16_t duty)
{
    const uint8_t pin = _pins[ch];
    if (pin == PIN_DISCONNECTED)
        return;
    duty = duty > 1000 ? 0 : duty; // prevent wraping around
    _activePwmChannels |= (1 << pin);
#if defined(PLATFORM_ESP32)
    ledcWrite(_chnMap[ch], map(duty, 0, 1000, 0, (1 << _resolution_bits[ch]) - 1));
#else
    uint16_t high = map(duty, 0, 1000, 0, _refreshInterval[ch]);
    startWaveform8266(pin, high, _refreshInterval[ch] - high);
#endif
}


void ServoMgr::setRefreshInterval(uint8_t ch, uint16_t intervalUs)
{
    if (intervalUs != 0)
    {
        _refreshInterval[ch] = intervalUs;
#if defined(PLATFORM_ESP32)
        const uint8_t pin = _pins[ch];
        if (pin == PIN_DISCONNECTED)
            return;
        allocateLedcChn(ch, intervalUs, pin); 
#endif
    }
}

void ServoMgr::stopPwm(uint8_t ch)
{
    const uint8_t pin = _pins[ch];
    if (pin == PIN_DISCONNECTED)
        return;
    _activePwmChannels &= ~(1 << pin);
#if defined(PLATFORM_ESP32)
    ledcDetachPin(pin);
#else
    stopWaveform8266(pin);
#endif
}

void ServoMgr::stopAllPwm()
{
    for (uint8_t ch=0; ch<_outputCnt; ++ch)
    {
        stopPwm(ch);
        const uint8_t pin = _pins[ch];
        digitalWrite(pin, LOW);
    }
    _activePwmChannels = 0;
}

void ServoMgr::writeDigital(uint8_t ch, bool value)
{
    const uint8_t pin = _pins[ch];
    if (pin == PIN_DISCONNECTED)
        return;
    if (isPwmActive(ch))
    {
        stopPwm(ch);
        // Wait for the last edge, which is at most 1 cycle from now
        delay((_refreshInterval[ch] / 1000U) + 1);
    }
    digitalWrite(pin, value);
}

#endif
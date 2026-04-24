#if defined(PLATFORM_ESP8266) || defined(PLATFORM_ESP32)

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

#if defined(PLATFORM_ESP32)
#include "driver/ledc.h"

#ifdef SOC_LEDC_SUPPORT_XTAL_CLOCK
#define LEDC_DEFAULT_CLK LEDC_USE_XTAL_CLK
#else
#define LEDC_DEFAULT_CLK LEDC_AUTO_CLK
#endif

extern uint8_t channels_resolution[];

/*
 * Modified versions of the ledcSetup/ledcAttachPin from Arduino ESP32 Hal which allows
 * control of the timer to use rather than being directly associated with the channel. This
 * allows multiple channels to use the same timer within a LEDC group (high/low speed) when
 * they are set to the same frequency.
 */
static void ledcSetupEx(uint8_t chan, ledc_timer_t timer, uint32_t freq, uint8_t bit_num)
{
    ledc_mode_t group = (ledc_mode_t)(chan / 8);

    ledc_timer_config_t ledc_timer = {
        .speed_mode = group,
        .duty_resolution = (ledc_timer_bit_t)bit_num,
        .timer_num = timer,
        .freq_hz = freq,
        .clk_cfg = LEDC_DEFAULT_CLK};
    if (ledc_timer_config(&ledc_timer) != ESP_OK)
    {
        log_e("ledc setup failed!");
        return;
    }
    channels_resolution[chan] = bit_num;
}

static void ledcAttachPinEx(uint8_t pin, uint8_t chan, ledc_timer_t timer)
{
    uint8_t group = (chan / 8), channel = (chan % 8);

    ledc_channel_config_t ledc_channel = {
        .gpio_num = pin,
        .speed_mode = (ledc_mode_t)group,
        .channel = (ledc_channel_t)channel,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = (ledc_timer_t)timer,
        .duty = 0,
        .hpoint = 0};
    ledc_channel_config(&ledc_channel);
}

void ServoMgr::allocateLedcChn(uint8_t ch, uint16_t intervalUs, uint8_t pin)
{
    uint32_t target_freq = 1000000U / intervalUs;
    _resolution_bits[ch] = (uint16_t)(log2f(80000000.0f / target_freq)); // no matter high speed timer or low speed timer, the clk src can be 80Mhz
    if (_resolution_bits[ch] > 16)
    {
        _resolution_bits[ch] = 16;
    }

    uint8_t group = ch / 8;
    for (int i = 0; i <= 3; i++)
    {
        uint8_t timer_idx = group * 4 + i;
        if (_timerConfigs[timer_idx] == 0)
        {
            _timerConfigs[timer_idx] = target_freq;
            ledcSetupEx(ch, (ledc_timer_t)i, target_freq, _resolution_bits[ch]);
        }
        if (_timerConfigs[timer_idx] == target_freq)
        {
            ledcAttachPinEx(pin, ch, (ledc_timer_t)i);
            DBGLN("allocate ledc_ch %d on pin %d using group: %d, ledc_tim: %d, bits: %d", ch, pin, group, i, _resolution_bits[ch]);
            return;
        }
    }
    DBGLN("Could not allocate timer for channel %d", ch);
}
#endif

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
#if defined(PLATFORM_ESP32)
    ledcWrite(ch, map(valueUs, 0, _refreshInterval[ch], 0, (1 << _resolution_bits[ch]) - 1));
#else
    startWaveform8266(pin, valueUs, _refreshInterval[ch] - valueUs);
#endif
}

void ServoMgr::writeDuty(uint8_t ch, uint16_t duty)
{
    const uint8_t pin = _pins[ch];
    if (pin == PIN_DISCONNECTED)
    {
        return;
    }
    _activePwmChannels |= (1 << ch);
#if defined(PLATFORM_ESP32)
    ledcWrite(ch, map(duty, 0, 1000, 0, (1 << _resolution_bits[ch]) - 1));
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
        {
            return;
        }
        allocateLedcChn(ch, intervalUs, pin);
#endif
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
#if defined(PLATFORM_ESP32)
    ledcDetachPin(pin);
#else
    stopWaveform8266(pin);
#endif
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
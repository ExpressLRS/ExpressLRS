#if defined(PLATFORM_ESP32_S3)

#include "ServoMgr.h"
#include "logging.h"
#include <math.h>

ServoMgr::ServoMgr(const uint8_t *const pins, const uint8_t outputCnt, uint32_t defaultInterval)
    : _pins(pins), _outputCnt(outputCnt), _refreshInterval(new uint16_t[outputCnt]), _activePwmChannels(0), _resolution_bits(new uint8_t[outputCnt])
{
    for (uint8_t ch = 0; ch < _outputCnt; ++ch)
    {
        _refreshInterval[ch] = defaultInterval;
    }
}

#include "driver/ledc.h"
#include "driver/mcpwm.h"

#define LEDC_CHANNELS   8
#define MCPWM_CHANNELS  12

#define IS_LEDC_CHANNEL(ch) (ch < LEDC_CHANNELS)
#define IS_MCPWM_CHANNEL(ch) (ch >= LEDC_CHANNELS && ch < LEDC_CHANNELS + MCPWM_CHANNELS)

#define LEDC_CHANNEL(ch) ch
#define MCPWM_CHANNEL(ch) (ch - LEDC_CHANNELS)

static const struct {
    mcpwm_unit_t unit;
    mcpwm_io_signals_t signal;
    mcpwm_timer_t timer;
    uint16_t freq;
} mcpwm_config[] = {
    {MCPWM_UNIT_0, MCPWM0A, MCPWM_TIMER_0, 0},
    {MCPWM_UNIT_0, MCPWM0B, MCPWM_TIMER_0, 0},
    {MCPWM_UNIT_0, MCPWM1A, MCPWM_TIMER_1, 0},
    {MCPWM_UNIT_0, MCPWM1B, MCPWM_TIMER_1, 0},
    {MCPWM_UNIT_0, MCPWM2A, MCPWM_TIMER_2, 0},
    {MCPWM_UNIT_0, MCPWM2B, MCPWM_TIMER_2, 0},
    {MCPWM_UNIT_1, MCPWM0A, MCPWM_TIMER_0, 0},
    {MCPWM_UNIT_1, MCPWM0B, MCPWM_TIMER_0, 0},
    {MCPWM_UNIT_1, MCPWM1A, MCPWM_TIMER_1, 0},
    {MCPWM_UNIT_1, MCPWM1B, MCPWM_TIMER_1, 0},
    {MCPWM_UNIT_1, MCPWM2A, MCPWM_TIMER_2, 0},
    {MCPWM_UNIT_1, MCPWM2B, MCPWM_TIMER_2, 0},
};

static int8_t mcpwm_map[MCPWM_CHANNELS] = {-1};

#ifdef SOC_LEDC_SUPPORT_XTAL_CLOCK
#define LEDC_DEFAULT_CLK LEDC_USE_XTAL_CLK
#else
#define LEDC_DEFAULT_CLK LEDC_AUTO_CLK
#endif

/*
 * Modified versions of the ledcSetup/ledcAttachPin from Arduino ESP32 Hal which allows
 * control of the timer to use rather than being directly associated with the channel. This
 * allows multiple channels to use the same timer within a LEDC group (high/low speed) when
 * they are set to the same frequency.
 */
static void ledcSetupEx(uint8_t chan, ledc_timer_t timer, uint32_t freq, uint8_t bit_num)
{
    ledc_timer_config_t ledc_timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = (ledc_timer_bit_t)bit_num,
        .timer_num = timer,
        .freq_hz = freq,
        .clk_cfg = LEDC_DEFAULT_CLK,
    };
    if (ledc_timer_config(&ledc_timer) != ESP_OK)
    {
        ERRLN("ledc setup failed!");
    }
}

static void ledcAttachPinEx(uint8_t pin, uint8_t chan, ledc_timer_t timer)
{
    ledc_channel_config_t ledc_channel = {
        .gpio_num = pin,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = (ledc_channel_t)chan,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = (ledc_timer_t)timer,
        .duty = 0,
        .hpoint = 0};
    ledc_channel_config(&ledc_channel);
}

void ServoMgr::allocateLedcChn(uint8_t ch, uint16_t intervalUs, uint8_t pin)
{
    uint32_t target_freq = 1000000U / intervalUs;
    _resolution_bits[ch] = (uint16_t)(log2f(80000000.0f / target_freq)); // clk src is 80Mhz
    if (_resolution_bits[ch] >= LEDC_TIMER_BIT_MAX)
    {
        _resolution_bits[ch] = LEDC_TIMER_BIT_MAX-1;
    }

    for (int timer_idx = 0; timer_idx <= 3; timer_idx++)
    {
        if (_timerConfigs[timer_idx] == 0)
        {
            _timerConfigs[timer_idx] = target_freq;
            ledcSetupEx(ch, (ledc_timer_t)timer_idx, target_freq, _resolution_bits[ch]);
        }
        if (_timerConfigs[timer_idx] == target_freq)
        {
            ledcAttachPinEx(pin, ch, (ledc_timer_t)timer_idx);
            DBGLN("allocate ledc_ch %d on pin %d using ledc_tim: %d, bits: %d", ch, pin, timer_idx, _resolution_bits[ch]);
            return;
        }
    }
    DBGLN("Could not allocate timer for channel %d", ch);
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
    if (IS_LEDC_CHANNEL(ch))
    {
        ledcWrite(LEDC_CHANNEL(ch), map(valueUs, 0, _refreshInterval[ch], 0, (1 << _resolution_bits[ch]) - 1));
    }
    else if (IS_MCPWM_CHANNEL(ch))
    {
        uint8_t channel = mcpwm_map[MCPWM_CHANNEL(ch)];
        mcpwm_set_duty_in_us(mcpwm_config[channel].unit, mcpwm_config[channel].timer, MCPWM_GEN_A, valueUs);
    }
}

void ServoMgr::writeDuty(uint8_t ch, uint16_t duty)
{
    const uint8_t pin = _pins[ch];
    if (pin == PIN_DISCONNECTED)
    {
        return;
    }
    _activePwmChannels |= (1 << ch);
    if (IS_LEDC_CHANNEL(ch))
    {
        ledcWrite(LEDC_CHANNEL(ch), map(duty, 0, 1000, 0, (1 << _resolution_bits[ch]) - 1));
    }
    else if (IS_MCPWM_CHANNEL(ch))
    {
        uint8_t channel = mcpwm_map[MCPWM_CHANNEL(ch)];
        mcpwm_set_duty(mcpwm_config[channel].unit, mcpwm_config[channel].timer, MCPWM_GEN_A, duty/10.0f);
    }
}

void ServoMgr::setRefreshInterval(uint8_t ch, uint16_t intervalUs)
{
    if (intervalUs != 0)
    {
        _refreshInterval[ch] = intervalUs;
        const uint8_t pin = _pins[ch];
        if (pin == PIN_DISCONNECTED)
        {
            return;
        }
        if (IS_LEDC_CHANNEL(ch))
        {
            allocateLedcChn(ch, intervalUs, pin);
        }
        else if (IS_MCPWM_CHANNEL(ch))
        {
            // allocateMCPWM(ch);
            // find matching freq or free channel
            int channel = -1;
            for (int i=0 ; i<MCPWM_CHANNELS ; i++)
            {
                if (mcpwm_config[i].freq == 1000000UL/intervalUs)
                {
                    if (i%2 == 0 && mcpwm_config[i+1].freq==0)
                    {
                        channel = i+1;
                        break;
                    }
                    if (i%2 == 1 && mcpwm_config[i-1].freq==0)
                    {
                        channel = i-1;
                        break;
                    }
                }
            }
            if (channel == -1)
            {
                for (int i=0 ; i<MCPWM_CHANNELS ; i+=2)
                {
                    if (mcpwm_config[i].freq == 0 && mcpwm_config[i+1].freq == 0)
                    {
                        channel = i;
                        break;
                    }
                }
            }
            if (channel == -1)
            {
                DBGLN("No MCPWM channel available");
                return;
            }
            mcpwm_map[MCPWM_CHANNEL(ch)] = channel;
            mcpwm_config_t pwm_config = {
                .frequency = 1000000UL/intervalUs,
                .cmpr_a = 0,
                .duty_mode = MCPWM_DUTY_MODE_0,
                .counter_mode = MCPWM_UP_COUNTER,
            };
            mcpwm_gpio_init(mcpwm_config[channel].unit, mcpwm_config[channel].signal, pin);
            mcpwm_init(mcpwm_config[channel].unit, mcpwm_config[channel].timer, &pwm_config);
        }
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
    if (IS_LEDC_CHANNEL(ch))
    {
        ledcDetachPin(pin);
    }
    else if (IS_MCPWM_CHANNEL(ch))
    {
        uint8_t channel = mcpwm_map[MCPWM_CHANNEL(ch)];
        mcpwm_stop(mcpwm_config[channel].unit, mcpwm_config[channel].timer);
    }
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
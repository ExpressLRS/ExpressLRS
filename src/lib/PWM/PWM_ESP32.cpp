#include "PWM.h"

#if defined(PLATFORM_ESP32) && !defined(PLATFORM_ESP32_S3)
#include <math.h>
#include <driver/ledc.h>

#include "logging.h"


#define LEDC_CHANNELS   16

#define LEDC_CHANNEL(ch) (ch & 0xFF)

static struct {
    int8_t pin;
    uint8_t resolution_bits;
    uint32_t interval;
} ledc_config[LEDC_CHANNELS] = {
    {-1, 0, 0}
};
uint32_t ledcTimerConfigs[LEDC_TIMER_MAX * LEDC_SPEED_MODE_MAX] = {0}; // 2 groups, 4 timers per group

/*
 * Modified versions of the ledcSetup/ledcAttachPin from Arduino ESP32 Hal which allows
 * control of the timer to use rather than being directly associated with the channel. This
 * allows multiple channels to use the same timer within a LEDC group (high/low speed) when
 * they are set to the same frequency.
 */
static void ledcSetupEx(uint8_t chan, ledc_mode_t group, ledc_timer_t timer, uint32_t freq, uint8_t bit_num)
{
    ledc_timer_config_t ledc_timer = {
        .speed_mode = group,
        .duty_resolution = (ledc_timer_bit_t)bit_num,
        .timer_num = timer,
        .freq_hz = freq,
        .clk_cfg = LEDC_USE_APB_CLK,
    };
    if (ledc_timer_config(&ledc_timer) != ESP_OK)
    {
        log_e("ledc setup failed!");
        return;
    }
}

static void ledcAttachPinEx(uint8_t pin, uint8_t chan, ledc_mode_t group, ledc_timer_t timer)
{
    ledc_channel_config_t ledc_channel = {
        .gpio_num = pin,
        .speed_mode = (ledc_mode_t)group,
        .channel = (ledc_channel_t)(chan % 8),
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = (ledc_timer_t)timer,
        .duty = 0,
        .hpoint = 0,
    };
    ledc_channel_config(&ledc_channel);
}

pwm_channel_t PWMController::allocate(uint8_t pin, uint32_t frequency)
{
    // 1. try for a LEDC channel
    auto bits = (uint8_t)(log2f(80000000.0f / frequency)); // clk src is 80Mhz
    if (bits >= LEDC_TIMER_BIT_MAX)
    {
        bits = LEDC_TIMER_BIT_MAX-1;
    }
    for (int ch=0 ; ch<LEDC_CHANNELS ; ch++)
    {
        if (ledc_config[ch].resolution_bits == 0)
        {
            ledc_mode_t group = ledc_mode_t(ch / 8);
            for (auto timer = 0; timer < 4; timer++)
            {
                uint8_t config_idx = group * 4 + timer;
                if (ledcTimerConfigs[config_idx] == 0)
                {
                    ledcTimerConfigs[config_idx] = frequency;
                    ledcSetupEx(ch, group, (ledc_timer_t)timer, frequency, bits);
                }
                if (ledcTimerConfigs[config_idx] == frequency)
                {
                    ledcAttachPinEx(pin, ch, group, (ledc_timer_t)timer);
                    ledc_config[ch].pin = pin;
                    ledc_config[ch].resolution_bits = bits;
                    ledc_config[ch].interval = 1000000U / frequency;
                    DBGLN("allocate ledc_ch %d on pin %d using group: %d, ledc_tim: %d, bits: %d", ch, pin, group, timer, bits);
                    return ch;
                }
            }
        }
    }

    // 2. bail out, nothing left
    DBGLN("No LEDC channels available for frequency %dHz", frequency);
    return -1;
}

void PWMController::release(pwm_channel_t channel)
{
    ledcDetachPin(ledc_config[channel].pin);
    ledc_config[channel].pin = -1;
    ledc_config[channel].resolution_bits = 0;
    ledc_config[channel].interval = 0;
}

void PWMController::setDuty(pwm_channel_t channel, uint16_t duty)
{
    ledcWrite(channel, map(duty, 0, 1000, 0, (1 << ledc_config[channel].resolution_bits) - 1));
}

void PWMController::setMicroseconds(pwm_channel_t channel, uint16_t microseconds)
{
    ledcWrite(channel, map(microseconds, 0, ledc_config[channel].interval, 0, (1 << ledc_config[channel].resolution_bits) - 1));
}

#endif
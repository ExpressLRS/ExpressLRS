#include "PWM.h"

#if defined(PLATFORM_ESP32)
#include <driver/ledc.h>
#include <driver/mcpwm.h>

#include "logging.h"

#if defined(PLATFORM_ESP32_S3) || defined(PLATFORM_ESP32_C3)
#define SPEED_MODE LEDC_LOW_SPEED_MODE
#else
#define SPEED_MODE LEDC_HIGH_SPEED_MODE
#endif

#define MCPWM_CHANNELS 12
#define LEDC_CHANNELS 8

#define MCPWM_CHANNEL_FLAG 0x100
#define LEDC_CHANNEL_FLAG 0x200

#define IS_MCPWM_CHANNEL(ch) (ch & MCPWM_CHANNEL_FLAG)
#define IS_LEDC_CHANNEL(ch) (ch & LEDC_CHANNEL_FLAG)

#define LEDC_CHANNEL(ch) (ch & 0xFF)
#define MCPWM_CHANNEL(ch) (ch & 0xFF)

#if SOC_MCPWM_SUPPORTED
static const struct
{
    mcpwm_unit_t unit;
    mcpwm_io_signals_t signal;
    mcpwm_timer_t timer;
    mcpwm_generator_t generator;
} mcpwm_config[] = {
    {MCPWM_UNIT_0, MCPWM0A, MCPWM_TIMER_0, MCPWM_GEN_A},
    {MCPWM_UNIT_0, MCPWM0B, MCPWM_TIMER_0, MCPWM_GEN_B},
    {MCPWM_UNIT_0, MCPWM1A, MCPWM_TIMER_1, MCPWM_GEN_A},
    {MCPWM_UNIT_0, MCPWM1B, MCPWM_TIMER_1, MCPWM_GEN_B},
    {MCPWM_UNIT_0, MCPWM2A, MCPWM_TIMER_2, MCPWM_GEN_A},
    {MCPWM_UNIT_0, MCPWM2B, MCPWM_TIMER_2, MCPWM_GEN_B},
    {MCPWM_UNIT_1, MCPWM0A, MCPWM_TIMER_0, MCPWM_GEN_A},
    {MCPWM_UNIT_1, MCPWM0B, MCPWM_TIMER_0, MCPWM_GEN_B},
    {MCPWM_UNIT_1, MCPWM1A, MCPWM_TIMER_1, MCPWM_GEN_A},
    {MCPWM_UNIT_1, MCPWM1B, MCPWM_TIMER_1, MCPWM_GEN_B},
    {MCPWM_UNIT_1, MCPWM2A, MCPWM_TIMER_2, MCPWM_GEN_A},
    {MCPWM_UNIT_1, MCPWM2B, MCPWM_TIMER_2, MCPWM_GEN_B},
};

static uint32_t mcpwm_frequencies[MCPWM_CHANNELS] = {0};
#endif

static struct
{
    int8_t pin;
    uint8_t resolution_bits;
    uint32_t interval;
} ledc_config[LEDC_CHANNELS];
static uint32_t ledcTimerConfigs[LEDC_TIMER_MAX] = {0};

/*
 * Modified versions of the ledcSetup/ledcAttachPin from Arduino ESP32 Hal which allows
 * control of the timer to use rather than being directly associated with the channel. This
 * allows multiple channels to use the same timer within a LEDC group (high/low speed) when
 * they are set to the same frequency.
 */
static void ledcSetupEx(uint8_t chan, ledc_timer_t timer, uint32_t freq, uint8_t bit_num)
{
    ledc_timer_config_t ledc_timer = {
        .speed_mode = SPEED_MODE,
        .duty_resolution = (ledc_timer_bit_t)bit_num,
        .timer_num = timer,
        .freq_hz = freq,
        .clk_cfg = LEDC_USE_APB_CLK,
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
        .speed_mode = SPEED_MODE,
        .channel = (ledc_channel_t)chan,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = (ledc_timer_t)timer,
        .duty = 0,
        .hpoint = 0};
    auto err = ledc_channel_config(&ledc_channel);
    if (err != OK)
    {
        ERRLN("ledc_channel_config failed with error 0x%x on pin %d", err, pin);
    }
}

pwm_channel_t PWMController::allocate(uint8_t pin, uint32_t frequency)
{
    // 1. see if we can allocate a MCPWM channel at this frequency
#if SOC_MCPWM_SUPPORTED
    // 1a. see if theres a MCPWM already using this frequency we can piggy-back on
    int channel = -1;

    for (int i = 0; i < MCPWM_CHANNELS; i++)
    {
        if (mcpwm_frequencies[i] == frequency)
        {
            if (i % 2 == 0 && mcpwm_frequencies[i + 1] == 0)
            {
                channel = i + 1;
                break;
            }
            if (i % 2 == 1 && mcpwm_frequencies[i - 1] == 0)
            {
                channel = i - 1;
                break;
            }
        }
    }
    if (channel == -1)
    {
        // 1b. check if theres an unassigned pair we can allocate to this frequency
        for (auto i = 0; i < MCPWM_CHANNELS; i += 2)
        {
            if (mcpwm_frequencies[i] == 0 && mcpwm_frequencies[i + 1] == 0)
            {
                channel = i;
                break;
            }
        }
    }
    if (channel != -1)
    {
        // 1c. got one, so configure the module to output the signal on the given pin
        mcpwm_config_t pwm_config = {
            .frequency = frequency,
            .cmpr_a = 0,
            .duty_mode = MCPWM_DUTY_MODE_0,
            .counter_mode = MCPWM_UP_COUNTER,
        };
        auto err = mcpwm_gpio_init(mcpwm_config[channel].unit, mcpwm_config[channel].signal, pin);
        if (err != ESP_OK)
        {
            DBGLN("mcpwm_gpio_init failed with error 0x%x on pin %d", err, pin);
        }
        mcpwm_init(mcpwm_config[channel].unit, mcpwm_config[channel].timer, &pwm_config);
        mcpwm_frequencies[channel] = frequency;
        return channel | MCPWM_CHANNEL_FLAG;
    }
#endif
    // 2. try for a LEDC channel
    for (int ch = 0; ch < LEDC_CHANNELS; ch++)
    {
        if (ledc_config[ch].resolution_bits == 0)
        {
            uint8_t bits = 0;
            uint32_t clock = 80000000U / frequency; // APB clk src is 80Mhz
            while (clock >>= 1)
            {
                ++bits;
            }
            if (bits >= LEDC_TIMER_BIT_MAX)
            {
                bits = LEDC_TIMER_BIT_MAX - 1;
            }
            for (auto timer_idx = 0; timer_idx < LEDC_TIMER_MAX; timer_idx++)
            {
                if (ledcTimerConfigs[timer_idx] == 0)
                {
                    ledcTimerConfigs[timer_idx] = frequency;
                    ledcSetupEx(ch, (ledc_timer_t)timer_idx, frequency, bits);
                }
                if (ledcTimerConfigs[timer_idx] == frequency)
                {
                    ledcAttachPinEx(pin, ch, (ledc_timer_t)timer_idx);
                    ledc_config[ch].pin = pin;
                    ledc_config[ch].resolution_bits = bits;
                    ledc_config[ch].interval = 1000000U / frequency;
                    DBGLN("allocate ledc_ch %d on pin %d using ledc_tim: %d, bits: %d", ch, pin, timer_idx, bits);
                    return ch | LEDC_CHANNEL_FLAG;
                }
            }
            break;
        }
    }

    // 3. bail out, nothing left
    DBGLN("No MCPWM or LEDC channels available for frequency %dHz", frequency);
    return -1;
}

void PWMController::release(pwm_channel_t channel)
{
    if (IS_LEDC_CHANNEL(channel))
    {
        auto ch = LEDC_CHANNEL(channel);
        ledcDetachPin(ledc_config[ch].pin);
        ledc_config[ch].pin = -1;
        ledc_config[ch].resolution_bits = 0;
        ledc_config[ch].interval = 0;
    }
#if SOC_MCPWM_SUPPORTED
    else if (IS_MCPWM_CHANNEL(channel))
    {
        auto ch = MCPWM_CHANNEL(channel);
        mcpwm_stop(mcpwm_config[ch].unit, mcpwm_config[ch].timer);
        mcpwm_frequencies[ch] = 0;
    }
#endif
    else
    {
        ERRLN("Invalid PWM channel %x", channel);
    }
}

void PWMController::setDuty(pwm_channel_t channel, uint16_t duty)
{
    if (IS_LEDC_CHANNEL(channel))
    {
        auto ch = LEDC_CHANNEL(channel);
        ledcWrite(ch, map(duty, 0, 1000, 0, (1 << ledc_config[ch].resolution_bits) - 1));
    }
#if SOC_MCPWM_SUPPORTED
    else if (IS_MCPWM_CHANNEL(channel))
    {
        auto ch = MCPWM_CHANNEL(channel);
        mcpwm_set_duty(mcpwm_config[ch].unit, mcpwm_config[ch].timer, mcpwm_config[ch].generator, duty / 10.0f);
    }
#endif
}

void PWMController::setMicroseconds(pwm_channel_t channel, uint16_t microseconds)
{
    if (IS_LEDC_CHANNEL(channel))
    {
        auto ch = LEDC_CHANNEL(channel);
        ledcWrite(ch, map(microseconds, 0, ledc_config[ch].interval, 0, (1 << ledc_config[ch].resolution_bits) - 1));
    }
#if SOC_MCPWM_SUPPORTED
    else if (IS_MCPWM_CHANNEL(channel))
    {
        auto ch = MCPWM_CHANNEL(channel);
        mcpwm_set_duty_in_us(mcpwm_config[ch].unit, mcpwm_config[ch].timer, mcpwm_config[ch].generator, microseconds);
    }
#endif
}

#endif
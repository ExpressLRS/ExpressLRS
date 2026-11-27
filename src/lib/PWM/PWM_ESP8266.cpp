#include "PWM.h"

#if defined(PLATFORM_ESP8266)

#include <math.h>
#include "waveform_8266.h"

#define MAX_PWM_CHANNELS    13

static uint16_t refreshInterval[MAX_PWM_CHANNELS] = {0};
static int8_t pwm_gpio[MAX_PWM_CHANNELS] = {-1};
static pwm_polarity_t pwm_polarity[MAX_PWM_CHANNELS] = {PWM_POLARITY_NORMAL};

pwm_channel_t PWMController::allocate(uint8_t pin, uint32_t frequency, pwm_polarity_t polarity)
{
    for(int channel=0 ; channel<MAX_PWM_CHANNELS ; channel++)
    {
        if (refreshInterval[channel] == 0)
        {
            pwm_gpio[channel] = pin;
            pwm_polarity[channel] = polarity;
            refreshInterval[channel] = 1000000U / frequency;
            digitalWrite(pin, polarity == PWM_POLARITY_INVERTED ? HIGH : LOW);
            pinMode(pin, OUTPUT);
            return channel;
        }
    }
    return -1;
}

void PWMController::release(pwm_channel_t channel)
{
    int8_t pin = pwm_gpio[channel];
    stopWaveform8266(pin);
    digitalWrite(pin, pwm_polarity[channel] == PWM_POLARITY_INVERTED ? HIGH : LOW);
    refreshInterval[channel] = 0;
    pwm_gpio[channel] = -1;
}

void PWMController::setDuty(pwm_channel_t channel, uint16_t duty)
{
    uint16_t high = map(duty, 0, 1000, 0, refreshInterval[channel]);
    setMicroseconds(channel, high);
}

void PWMController::setMicroseconds(pwm_channel_t channel, uint16_t microseconds)
{
    int8_t pin = pwm_gpio[channel];
    const bool inverted = pwm_polarity[channel] != PWM_POLARITY_NORMAL;
    if (microseconds == 0)
    {
        stopWaveform8266(pin);
        digitalWrite(pin, pwm_polarity[channel] == PWM_POLARITY_INVERTED ? HIGH : LOW);
        return;
    }
    if (microseconds == refreshInterval[channel])
    {
        stopWaveform8266(pin);
        digitalWrite(pin, inverted ? LOW : HIGH);
        return;
    }
    const uint16_t rest = refreshInterval[channel] - microseconds;
    startWaveform8266(pin, inverted ? rest : microseconds, inverted ? microseconds : rest);
}

void PWMController::feedWatchdog()
{
    feedWaveform8266();
}

#endif

#include "PWM.h"

#if defined(PLATFORM_ESP8266)

#include <math.h>
#include "waveform_8266.h"

#define MAX_PWM_CHANNELS    13

static uint16_t refreshInterval[MAX_PWM_CHANNELS] = {0};
static int8_t pwm_gpio[MAX_PWM_CHANNELS] = {-1};

pwm_channel_t PWMController::allocate(uint8_t pin, uint32_t frequency)
{
    for(int channel=0 ; channel<MAX_PWM_CHANNELS ; channel++)
    {
        if (refreshInterval[channel] == 0)
        {
            pwm_gpio[channel] = pin;
            refreshInterval[channel] = 1000000U / frequency;
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
    digitalWrite(pin, LOW);
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
    if (microseconds > 0) {
        startWaveform8266(pin, microseconds, refreshInterval[channel] - microseconds);
    }
    else {
        // startWaveform8266 does not handle 0 properly, there's still a 1.2 microsecond pulse
        // so we have to explicitly stop the waveform generation
        stopWaveform8266(pin);
        digitalWrite(pin, LOW);
    }
}

#endif
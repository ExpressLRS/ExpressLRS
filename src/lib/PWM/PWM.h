#include <Arduino.h>

typedef int pwm_channel_t;

class PWMController
{
public:
    /**
     * @brief Allocate a pin/frequency to a PWM channel
     *
     * @param pin the GPIO pin to output the PWM signal on
     * @param frequency the output frequency of the PWM signal
     * @return the allocated PWM channel or -1 if a channel cannot be assigned
     */
    pwm_channel_t allocate(uint8_t pin, uint32_t frequency);

    /**
     * @brief release a channel back to the allocater
     *
     * @param channel the channel to release
     */
    void release(pwm_channel_t channel);

    /**
     * @brief Set the duty cycle of the PWM signal
     * The duty is in one-tenths of percent (i.e. value is 0-1000)
     *
     * @param channel the channel to adjust the signal on
     * @param duty the high time percentage in tenths of a percent
     */
    void setDuty(pwm_channel_t channel, uint16_t duty);

    /**
     * @brief Set the output PWM signal high for the supplied microseconds
     *
     * @param channel the channel to adjust the signal on
     * @param microseconds the high time in microsends
     */
    void setMicroseconds(pwm_channel_t channel, uint16_t microseconds);
};
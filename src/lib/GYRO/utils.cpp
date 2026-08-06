#include "targets.h"

#if defined(GYRO_SUPPORT)
#include "config.h"
#include "utils.h"
#include "gyro.h"
#include "logging.h"
#include "crsf_protocol.h"


/**
 *  Apply a correction to a servo PWM value
 */
float us_command_to_float(uint16_t us)
{
    // TODO: this will take into account subtrim and max throws
    return us <= GYRO_US_MID
        ? float(us - GYRO_US_MID) / (GYRO_US_MID - GYRO_US_MIN)
        : float(us - GYRO_US_MID) / (GYRO_US_MAX - GYRO_US_MID);
}

/**
 * Convert a CRSF value to a float
 */
float crsf_command_to_float(uint16_t command)
{
    return command <= CRSF_CHANNEL_VALUE_MID
        ? float (command - CRSF_CHANNEL_VALUE_MID) / (CRSF_CHANNEL_VALUE_MID - CRSF_CHANNEL_VALUE_MIN)
        : float (command - CRSF_CHANNEL_VALUE_MID) / (CRSF_CHANNEL_VALUE_MAX - CRSF_CHANNEL_VALUE_MID);
}

/**
 * Convert a channel µs value to a float command
 *
 * This takes into account subtrim and max gyro throws.
 */
float us_command_to_float(uint8_t ch, uint16_t us)
{
    const rx_config_pwm_limits_t *limits = config.GetPwmChannelLimits(ch);
    const uint16_t mid = limits->val.mid;
    return us <= mid
        ? float(us - mid) / (mid - limits->val.min)
        : float(us - mid) / (limits->val.max - mid);
}

/**
 * Convert +-1.0 float into µs for an output channel
 *
 * This takes into account subtrim and max gyro throws.
 */
uint16_t float_to_us(uint8_t ch, float value)
{
    const rx_config_pwm_limits_t *limits = config.GetPwmChannelLimits(ch);
    const uint16_t mid = limits->val.mid;

    return value < 0
        ? mid + ((mid - limits->val.min) * value)
        : mid + ((limits->val.max - mid) * value);
}
#endif

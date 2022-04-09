#if defined(GPIO_PIN_PWM_OUTPUTS)
#include "devServoOutput.h"
#include "common.h"
#include "config.h"
#include "CRSF.h"
#include "helpers.h"

static constexpr uint8_t SERVO_PINS[] = GPIO_PIN_PWM_OUTPUTS;
static constexpr uint8_t SERVO_COUNT = ARRAY_SIZE(SERVO_PINS);
static Servo *Servos[SERVO_COUNT];
static bool newChannelsAvailable;

void ICACHE_RAM_ATTR servoNewChannelsAvaliable()
{
    newChannelsAvailable = true;
}

static void servosFailsafe()
{
    for (uint8_t ch=0; ch<SERVO_COUNT; ++ch)
    {
        // Note: Failsafe values do not respect the inverted flag, failsafes are absolute
        uint16_t us = config.GetPwmChannel(ch)->val.failsafe + 988U;
        if (Servos[ch])
            Servos[ch]->writeMicroseconds(us);
    }
}

static int servosUpdate(unsigned long now)
{
    // The ESP waveform generator is nice because it doesn't change the value
    // mid-cycle, but it does busywait if there's already a change queued.
    // Updating every 20ms minimizes the amount of waiting (0-800us cycling
    // after it syncs up) where 19ms always gets a 1000-1800us wait cycling
    static uint32_t lastUpdate;
    const uint32_t elapsed = now - lastUpdate;
    if (elapsed < 20)
        return DURATION_IMMEDIATELY;

    if (newChannelsAvailable)
    {
        newChannelsAvailable = false;
        for (uint8_t ch=0; ch<SERVO_COUNT; ++ch)
        {
            const rx_config_pwm_t *chConfig = config.GetPwmChannel(ch);
            uint16_t us = CRSF_to_US(CRSF::GetChannelOutput(chConfig->val.inputChannel));
            if (chConfig->val.inverted)
                us = 3000U - us;

            if (Servos[ch])
                Servos[ch]->writeMicroseconds(us);
            else if (us >= 988U && us <= 2012U)
            {
                // us might be out of bounds if this is a switch channel and it has not been
                // received yet. Delay initializing the servo until the channel is valid
                Servo *servo = new Servo();
                Servos[ch] = servo;
                servo->attach(SERVO_PINS[ch], 988U, 2012U, us);
            }
        } /* for each servo */
    } /* if newChannelsAvailable */

    else if (elapsed > 1000U && connectionState == connected)
    {
        // No update for 1s, go to failsafe
        servosFailsafe();
    }

    else
        return DURATION_IMMEDIATELY; // prevent updating lastUpdate

    // need to sample actual millis at the end to account for any
    // waiting that happened in Servo::writeMicroseconds()
    lastUpdate = millis();
    return DURATION_IMMEDIATELY;
}

static void initialize()
{
    // Initialize all servos to low ASAP
    for (uint8_t ch=0; ch<SERVO_COUNT; ++ch)
    {
        pinMode(SERVO_PINS[ch], OUTPUT);
        digitalWrite(SERVO_PINS[ch], LOW);
    }
}

static int start()
{
    return DURATION_NEVER; // or maybe 500ms needed?
}

static int event()
{
    if (connectionState == disconnected)
        return DURATION_NEVER;
    else
        return DURATION_IMMEDIATELY;
}

static int timeout()
{
    return servosUpdate(millis());
}

device_t ServoOut_device = {
  .initialize = initialize,
  .start = start,
  .event = event,
  .timeout = timeout
};

#endif


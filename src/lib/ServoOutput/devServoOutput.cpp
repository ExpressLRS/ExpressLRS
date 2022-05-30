#if defined(GPIO_PIN_PWM_OUTPUTS)
#include "devServoOutput.h"
#include "common.h"
#include "config.h"
#include "CRSF.h"
#include "helpers.h"

static uint8_t SERVO_PINS[PWM_MAX_CHANNELS];
static ServoMgr_8266 *servoMgr;
// true when the RX has a new channels packet
static bool newChannelsAvailable;

void ICACHE_RAM_ATTR servoNewChannelsAvaliable()
{
    newChannelsAvailable = true;
}

uint16_t servoOutputModeToUs(eServoOutputMode mode)
{
    switch (mode)
    {
        case som50Hz: return (1000000U / 50U);
        case som60Hz: return (1000000U / 60U);
        case som100Hz: return (1000000U / 100U);
        case som160Hz: return (1000000U / 160U);
        case som333Hz: return (1000000U / 333U);
        case som400Hz: return (1000000U / 400U);
        default:
            return 0;
    }
}

static void servosFailsafe()
{
    for (unsigned ch=0; ch<servoMgr->getOutputCnt(); ++ch)
    {
        const rx_config_pwm_t *chConfig = config.GetPwmChannel(ch);
        // Note: Failsafe values do not respect the inverted flag, failsafes are absolute
        uint16_t us = chConfig->val.failsafe + 988U;
        // Always write the failsafe position even if the servo never has been started,
        // so all the servos go to their expected position
        servoMgr->writeMicroseconds(ch, us / (chConfig->val.narrow + 1));
    }
}

static int servosUpdate(unsigned long now)
{
    static uint32_t lastUpdate;
    if (newChannelsAvailable)
    {
        newChannelsAvailable = false;
        lastUpdate = now;
        for (unsigned ch=0; ch<servoMgr->getOutputCnt(); ++ch)
        {
            const rx_config_pwm_t *chConfig = config.GetPwmChannel(ch);
            const unsigned crsfVal = CRSF::GetChannelOutput(chConfig->val.inputChannel);
            // crsfVal might 0 if this is a switch channel and it has not been
            // received yet. Delay initializing the servo until the channel is valid
            if (crsfVal == 0)
                continue;
            uint16_t us = CRSF_to_US(crsfVal);
            // Flip the output around the mid value if inverted
            // (1500 - usOutput) + 1500
            if (chConfig->val.inverted)
                us = 3000U - us;

            if ((eServoOutputMode)chConfig->val.mode == somOnOff)
                servoMgr->writeDigital(ch, us > 1500U);
            else
                servoMgr->writeMicroseconds(ch, us / (chConfig->val.narrow + 1));
        } /* for each servo */
    } /* if newChannelsAvailable */

    else if (lastUpdate && (now - lastUpdate) > 1000U && connectionState == connected)
    {
        // No update for 1s, go to failsafe
        servosFailsafe();
        lastUpdate = 0;
    }

    return DURATION_IMMEDIATELY;
}

static void initialize()
{
    if (!OPT_HAS_SERVO_OUTPUT)
        return;

    servoMgr = new ServoMgr_8266(SERVO_PINS, GPIO_PIN_PWM_OUTPUTS_COUNT, 20000U);

    for (unsigned ch=0; ch<servoMgr->getOutputCnt(); ++ch)
    {
        uint8_t pin = GPIO_PIN_PWM_OUTPUTS[ch];
#if defined(DEBUG_LOG) && defined(PLATFORM_ESP8266)
        // Disconnect the debug UART pins if DEBUG_LOG
        if (pin == 1 || pin == 3)
        {
            pin = servoMgr->PIN_DISCONNECTED;
        }
#endif
        SERVO_PINS[ch] = pin;
    }

    // Initialize all servos to low ASAP
    servoMgr->initialize();
}

static int start()
{
    for (unsigned ch=0; servoMgr && ch<servoMgr->getOutputCnt(); ++ch)
    {
        const rx_config_pwm_t *chConfig = config.GetPwmChannel(ch);
        servoMgr->setRefreshInterval(ch, servoOutputModeToUs((eServoOutputMode)chConfig->val.mode));
    }

    return DURATION_NEVER;
}

static int event()
{
    if (servoMgr == nullptr || connectionState == disconnected)
    {
        // Disconnected should come after failsafe on the RX
        // so it is safe to shut down when disconnected
        return DURATION_NEVER;
    }
    else if (connectionState == wifiUpdate)
    {
        servoMgr->stopAllPwm();
        return DURATION_NEVER;
    }
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

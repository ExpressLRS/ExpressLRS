#if defined(GPIO_PIN_PWM_OUTPUTS)
#include "devServoOutput.h"
#include "PWM.h"
#include "CRSF.h"
#include "config.h"
#include "logging.h"
#include "rxtx_intf.h"

static int8_t servoPins[PWM_MAX_CHANNELS];
static pwm_channel_t pwmChannels[PWM_MAX_CHANNELS];
static uint16_t pwmChannelValues[PWM_MAX_CHANNELS];
static int pwmInputChannels[PWM_MAX_CHANNELS] = {-1};

#if (defined(PLATFORM_ESP32))
static DShotRMT *dshotInstances[PWM_MAX_CHANNELS] = {nullptr};
const uint8_t RMT_MAX_CHANNELS = 8;
#endif

// true when the RX has a new channels packet
static bool newChannelsAvailable;
// Absolute max failsafe time if no update is received, regardless of LQ
static constexpr uint32_t FAILSAFE_ABS_TIMEOUT_MS = 1000U;

constexpr unsigned SERVO_FAILSAFE_MIN = 886U;

void ICACHE_RAM_ATTR servoNewChannelsAvailable()
{
    newChannelsAvailable = true;
}

uint16_t servoOutputModeToFrequency(eServoOutputMode mode)
{
    switch (mode)
    {
    case som50Hz:
        return 50U;
    case som60Hz:
        return 60U;
    case som100Hz:
        return 100U;
    case som160Hz:
        return 160U;
    case som333Hz:
        return 333U;
    case som400Hz:
        return 400U;
    case som10KHzDuty:
        return 10000U;
    default:
        return 0;
    }
}

static void servoWrite(uint8_t ch, uint16_t us)
{
    const rx_config_pwm_t *chConfig = config.GetPwmChannel(ch);
#if defined(PLATFORM_ESP32)
    if ((eServoOutputMode)chConfig->val.mode == somDShot)
    {
        // DBGLN("Writing DShot output: us: %u, ch: %d", us, ch);
        if (dshotInstances[ch])
        {
            dshotInstances[ch]->send_dshot_value(((us - 1000) * 2) + 47); // Convert PWM signal in us to DShot value
        }
    }
    else
#endif
    if (servoPins[ch] != UNDEF_PIN && pwmChannelValues[ch] != us)
    {
        pwmChannelValues[ch] = us;
        if ((eServoOutputMode)chConfig->val.mode == somOnOff)
        {
            digitalWrite(servoPins[ch], us > 1500);
        }
        else if ((eServoOutputMode)chConfig->val.mode == som10KHzDuty)
        {
            PWM.setDuty(pwmChannels[ch], constrain(us, 1000, 2000) - 1000);
        }
        else
        {
            PWM.setMicroseconds(pwmChannels[ch], us / (chConfig->val.narrow + 1));
        }
    }
}

static void servosFailsafe()
{
    constexpr unsigned SERVO_FAILSAFE_MIN = 988U;
    for (int ch = 0 ; ch < GPIO_PIN_PWM_OUTPUTS_COUNT ; ++ch)
    {
        const rx_config_pwm_t *chConfig = config.GetPwmChannel(ch);
        if (chConfig->val.failsafeMode == PWMFAILSAFE_SET_POSITION) {
            // Note: Failsafe values do not respect the inverted flag, failsafe values are absolute
            uint16_t us = chConfig->val.failsafe + SERVO_FAILSAFE_MIN;
            // Always write the failsafe position even if the servo has never been started,
            // so all the servos go to their expected position
            servoWrite(ch, us);
        }
        else if (chConfig->val.failsafeMode == PWMFAILSAFE_NO_PULSES) {
            servoWrite(ch, 0);
        }
        else if (chConfig->val.failsafeMode == PWMFAILSAFE_LAST_POSITION) {
            // do nothing
        }
    }
}

static void servosUpdate(unsigned long now)
{
    static uint32_t lastUpdate;
    if (newChannelsAvailable)
    {
        newChannelsAvailable = false;
        lastUpdate = now;
        for (int ch = 0 ; ch < GPIO_PIN_PWM_OUTPUTS_COUNT ; ++ch)
        {
            const rx_config_pwm_t *chConfig = config.GetPwmChannel(ch);
            const unsigned crsfVal = ChannelData[chConfig->val.inputChannel];
            // crsfVal might 0 if this is a switch channel, and it has not been
            // received yet. Delay initializing the servo until the channel is valid
            if (crsfVal == 0)
            {
                continue;
            }

            uint16_t us = CRSF_to_US(crsfVal);
            // Flip the output around the mid-value if inverted
            // (1500 - usOutput) + 1500
            if (chConfig->val.inverted)
            {
                us = 3000U - us;
            }
            //
            if (us < 1050U){
                servoWrite(ch, SERVO_FAILSAFE_MIN);
            } else if (us > 1500 && us < 1600){
                servoWrite(ch, 1450);
            } else {
                servoWrite(ch, us);
            }
        } /* for each servo */
    }     /* if newChannelsAvailable */

    // LQ goes to 0 (100 packets missed in a row)
    // OR last update older than FAILSAFE_ABS_TIMEOUT_MS
    // go to failsafe
    else if (lastUpdate && ((getLq() == 0) || (now - lastUpdate > FAILSAFE_ABS_TIMEOUT_MS)))
    {
        servosFailsafe();
        lastUpdate = 0;
    }
}

static void initialize()
{
    if (!OPT_HAS_SERVO_OUTPUT)
    {
        return;
    }

#if defined(M0139)
    PWM.initialize();
#endif //M0139

#if defined(PLATFORM_ESP32)
    uint8_t rmtCH = 0;
#endif
    for (int ch = 0; ch < GPIO_PIN_PWM_OUTPUTS_COUNT; ++ch)
    {
        pwmChannelValues[ch] = UINT16_MAX;
        pwmChannels[ch] = -1;
        int8_t pin = GPIO_PIN_PWM_OUTPUTS[ch];
#if (defined(DEBUG_LOG) || defined(DEBUG_RCVR_LINKSTATS)) && (defined(PLATFORM_ESP8266) || defined(PLATFORM_ESP32))
        // Disconnect the debug UART pins if DEBUG_LOG
        if (pin == U0RXD_GPIO_NUM || pin == U0TXD_GPIO_NUM)
        {
            pin = UNDEF_PIN;
        }
#endif
        // Mark servo pins that are being used for serial (or other purposes) as disconnected
        auto mode = (eServoOutputMode)config.GetPwmChannel(ch)->val.mode;
        if (mode >= somSerial)
        {
            pin = UNDEF_PIN;
        }
#if defined(PLATFORM_ESP32)
        else if (mode == somDShot)
        {
            if (rmtCH < RMT_MAX_CHANNELS)
            {
                auto gpio = (gpio_num_t)pin;
                auto rmtChannel = (rmt_channel_t)rmtCH;
                DBGLN("Initializing DShot: gpio: %u, ch: %d, rmtChannel: %u", gpio, ch, rmtChannel);
                pinMode(pin, OUTPUT);
                dshotInstances[ch] = new DShotRMT(gpio, rmtChannel); // Initialize the DShotRMT instance
                rmtCH++;
            }
            pin = UNDEF_PIN;
        }
#endif
        servoPins[ch] = pin;
        // Initialize all servos to low ASAP
        if (pin != UNDEF_PIN)
        {
            if (mode == somOnOff)
            {
                DBGLN("Initializing digital output: ch: %d, pin: %d", ch, pin);
            }
            else
            {
                DBGLN("Initializing PWM output: ch: %d, pin: %d", ch, pin);
            }
#ifndef M0139
            pinMode(pin, OUTPUT);
            digitalWrite(pin, LOW);
#endif // !M0139
        }
    }
}

static int start()
{
    for (int ch = 0; ch < GPIO_PIN_PWM_OUTPUTS_COUNT; ++ch)
    {
        const rx_config_pwm_t *chConfig = config.GetPwmChannel(ch);
        auto frequency = servoOutputModeToFrequency((eServoOutputMode)chConfig->val.mode);
        if (frequency && servoPins[ch] != UNDEF_PIN)
        {
            pwmChannels[ch] = PWM.allocate(servoPins[ch], frequency);
            pwmInputChannels[ch] = chConfig->val.inputChannel;
        }
#if defined(PLATFORM_ESP32)
        else if (((eServoOutputMode)chConfig->val.mode) == somDShot)
        {
            dshotInstances[ch]->begin(DSHOT300, false); // Set DShot protocol and bidirectional dshot bool
            dshotInstances[ch]->send_dshot_value(0);         // Set throttle low so the ESC can continue initialsation
        }
#endif
    }
    return DURATION_NEVER;
}

static int event()
{
    // Change pwm value from telemetry command
    if (updatePWM && (PWMCmd)pwmCmd == PWMCmd::SET_PWM_VAL){
        updatePWM = false;

        for (int ch = 0; ch < GPIO_PIN_PWM_OUTPUTS_COUNT; ++ch)
        {
            if(pwmInputChannels[ch] == pwmInputChannel)
            {
                if (pwmChannels[ch] == -1){
                    return DURATION_IMMEDIATELY;
                }

                if (pwmType == 's'){
                    PWM.setMicroseconds(pwmChannels[ch], pwmValue);
                }
                else if (pwmType == 'd'){
                    PWM.setDuty(pwmChannels[ch], pwmValue);
                }
            }
        }
    // Change pwm channel from telemetry command
    } else if (updatePWM && (PWMCmd)pwmCmd == PWMCmd::SET_PWM_CH){
        updatePWM = false;

        int8_t pin = -1;
        switch(pwmPin) {
            case 0:
                pin = Ch1;
                break;
            case 1:
                pin = Ch2;
                break;
            case 2:
                pin = Ch3;
                break;
            case 3:
                pin = Ch4;
                break;
        }
        
        for (int ch = 0; ch < GPIO_PIN_PWM_OUTPUTS_COUNT; ++ch)
        {
            // Skip changing channel if the new channel is the same as the old one
            if(servoPins[ch] == pin && pwmInputChannels[ch] != pwmInputChannel)
            {
                // TODO: Allow setting the mode and failsafe as well
                // Release the pin from its old allocation (unnecessary if not changing modes so commented out)
                //PWM.release(pin);
                //pwmChannels[ch] = PWM.allocate(pin, 50U);

                // Set the new channel
                config.SetPwmChannel(ch, 0, pwmInputChannel, false, som50Hz, false);
                pwmInputChannels[ch] = pwmInputChannel;
                break;
            }
        }
    }

    if (!OPT_HAS_SERVO_OUTPUT || connectionState == disconnected)
    {
        // Disconnected should come after failsafe on the RX,
        // so it is safe to shut down when disconnected
        return DURATION_NEVER;
    }
    else if (connectionState == wifiUpdate)
    {
        for (int ch = 0; ch < GPIO_PIN_PWM_OUTPUTS_COUNT; ++ch)
        {
            if (pwmChannels[ch] != -1)
            {
                PWM.release(pwmChannels[ch]);
                pwmChannels[ch] = -1;
            }
#if defined(PLATFORM_ESP32)
            if (dshotInstances[ch] != nullptr)
            {
                delete dshotInstances[ch];
                dshotInstances[ch] = nullptr;
            }
#endif
            servoPins[ch] = UNDEF_PIN;
        }
        return DURATION_NEVER;
    }
    return DURATION_IMMEDIATELY;
}

static int timeout()
{
    servosUpdate(millis());
    return DURATION_IMMEDIATELY;
}

device_t ServoOut_device = {
    .initialize = initialize,
    .start = start,
    .event = event,
    .timeout = timeout,
};

#endif

#pragma once

#if defined(GPIO_PIN_PWM_OUTPUTS)
#include "device.h"
#include "ServoMgr_8266.h"

enum eServoOutputMode {
    som50Hz,  // Hz modes are "Servo PWM" where the signal is 988-2012us
    som60Hz,  // and the mode sets the refresh interval
    som100Hz,
    som160Hz,
    som333Hz,
    som400Hz,
    somOnOff, // Digital 0/1 mode
    somPwm    // True PWM mode (NOT SUPPORTED)
};

extern device_t ServoOut_device;
#define HAS_SERVO_OUTPUT

// Notify this unit that new channel data has arrived
void servoNewChannelsAvaliable();
#endif

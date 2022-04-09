#pragma once

#if defined(GPIO_PIN_PWM_OUTPUTS)
#include "device.h"
#include <Servo.h>

extern device_t ServoOut_device;
#define HAS_SERVO_OUTPUT

// Notify this unit that new channel data has arrived
void servoNewChannelsAvaliable();
#endif

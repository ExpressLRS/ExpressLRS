#pragma once
#if defined(GPIO_PIN_PWM_OUTPUTS)

#include "ServoMgr.h"
#include "DShotRMT.h"
#include "device.h"
#include "common.h"

extern device_t ServoOut_device;
#define HAS_SERVO_OUTPUT
#define OPT_HAS_SERVO_OUTPUT (GPIO_PIN_PWM_OUTPUTS_COUNT > 0)

// Notify this unit that new channel data has arrived
void servoNewChannelsAvaliable();
// Convert eServoOutputMode to microseconds, or 0 for non-servo modes
uint16_t servoOutputModeToUs(eServoOutputMode mode);
#else
inline void servoNewChannelsAvaliable(){};
#endif

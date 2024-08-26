#pragma once
#if defined(GPIO_PIN_PWM_OUTPUTS)

#include "device.h"
#include "common.h"
#include "telemetry.h"

#if (defined(PLATFORM_ESP32))
#include "DShotRMT.h"
#endif

extern device_t ServoOut_device;
#define HAS_SERVO_OUTPUT
#define OPT_HAS_SERVO_OUTPUT (GPIO_PIN_PWM_OUTPUTS_COUNT > 0)


extern bool updatePWM;
extern uint8_t pwmPin;
extern uint8_t pwmCmd;
extern uint8_t pwmOutputChannel;
extern uint8_t pwmInputChannel;
extern uint8_t pwmType;
extern uint16_t pwmValue; 

// Notify this unit that new channel data has arrived
void servoNewChannelsAvailable();
#else
inline void servoNewChannelsAvailable(){};
#endif

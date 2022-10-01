#pragma once

#if defined(GPIO_PIN_PWM_OUTPUTS)
#include "device.h"
#include "ServoMgr.h"

enum eServoOutputMode {
    som50Hz,  // Hz modes are "Servo PWM" where the signal is 988-2012us
    som60Hz,  // and the mode sets the refresh interval
    som100Hz, // 50Hz must be mode=0 for default in config
    som160Hz,
    som333Hz,
    som400Hz,
    som10KHz,
    somOnOff,   // Digital 0/1 mode
    somPwm,     // True PWM mode (NOT SUPPORTED)
    somCrsfTx,  // CRSF output TX (NOT SUPPORTED)
    somCrsfRx,  // CRSF output RX (NOT SUPPORTED)
};

enum eServerPulseWidthMode{
    normal,
    narrow,
    duty
};


extern device_t ServoOut_device;
#define HAS_SERVO_OUTPUT
#define OPT_HAS_SERVO_OUTPUT (GPIO_PIN_PWM_OUTPUTS_COUNT > 0)

// Notify this unit that new channel data has arrived
void servoNewChannelsAvaliable();
// Convert eServoOutputMode to microseconds, or 0 for non-servo modes
uint16_t servoOutputModeToUs(eServoOutputMode mode);
#else
inline void servoNewChannelsAvaliable() {};
#endif

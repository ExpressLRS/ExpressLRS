#pragma once

#if defined(PLATFORM_ESP32)

#include "targets.h"
#include <BleGamepad.h>

#include "CRSF.h"
extern CRSF crsf;

#define numOfButtons 0
#define numOfHatSwitches 0
#define enableX true
#define enableY true
#define enableZ false
#define enableRZ false
#define enableRX true
#define enableRY true
#define enableSlider1 true
#define enableSlider2 true
#define enableRudder true
#define enableThrottle true
#define enableAccelerator false
#define enableBrake false
#define enableSteering false

BleGamepad bleGamepad("ExpressLRS Joystick", "ELRS", 100);

void BluetoothJoystickBegin()
{
    bleGamepad.setAutoReport(false);
    Serial.println("Starting BLE Joystick!");
    bleGamepad.setControllerType(CONTROLLER_TYPE_GAMEPAD);
    bleGamepad.begin(numOfButtons, numOfHatSwitches, enableX, enableY, enableZ, enableRZ, enableRX, enableRY, enableSlider1, enableSlider2, enableRudder, enableThrottle, enableAccelerator, enableBrake, enableSteering);
}

void BluetoothJoystickUpdateValues()
{
    if (bleGamepad.isConnected())
    {
        int16_t data[sizeof(crsf.ChannelDataIn)] = {0};

        for (uint8_t i = 0; i < 9; i++)
        {
            data[i] = map(crsf.ChannelDataIn[i], CRSF_CHANNEL_VALUE_MIN - 1, CRSF_CHANNEL_VALUE_MAX + 1, -32768, 32768);
        }

        bleGamepad.setX(data[0]);
        bleGamepad.setY(data[1]);
        bleGamepad.setRX(data[2]);
        bleGamepad.setRY(data[3]);
        bleGamepad.setRudder(data[4]);
        bleGamepad.setThrottle(data[5]);
        bleGamepad.setSlider1(data[6]);
        bleGamepad.setSlider2(data[7]);

        bleGamepad.sendReport();
    }
}

#endif
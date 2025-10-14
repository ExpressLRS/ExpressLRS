#include "devBLE.h"

#if defined(PLATFORM_ESP32)

#include "common.h"
#include "crsf_protocol.h"
#include "handset.h"
#include "POWERMGNT.h"
#include "hwTimer.h"
#include "logging.h"
#include "devButton.h"

#include <BleGamepad.h>

static BleGamepad *bleGamepad = nullptr;

void BluetoothJoystickUpdateValues()
{
    if (bleGamepad->isConnected())
    {
        // map first 8 channels to axis
        int16_t data[8];
        for (uint8_t i = 0; i < 8; i++)
        {
            data[i] = map(ChannelData[i], CRSF_CHANNEL_VALUE_MIN, CRSF_CHANNEL_VALUE_MAX, bleGamepad->configuration.getAxesMin(), bleGamepad->configuration.getAxesMax());
        }
        bleGamepad->setAxes(data[0], data[1], data[4], data[5], data[2], data[3], data[6], data[7]);

        // map other 8 channels to buttons
        for (uint8_t i = 8; i < 16; i++)
        {
            if (ChannelData[i] >= CRSF_CHANNEL_VALUE_2000)
            {
                bleGamepad->press(i - 7);
            }
            else
            {
                bleGamepad->release(i - 7);
            }
        }

        // send BLE report
        bleGamepad->sendReport();
    }
}

void BluetoothJoystickBegin()
{
    // bleGamepad is null if it hasn't been started yet
    if (bleGamepad != nullptr) return;

    DBGLN("Starting BLE Joystick");

    POWERMGNT::setPower(MinPower);
    Radio.End();

    BleGamepadConfiguration gamepadConfig;
    gamepadConfig.setAutoReport(false);
    gamepadConfig.setControllerType(CONTROLLER_TYPE_JOYSTICK); // CONTROLLER_TYPE_JOYSTICK, CONTROLLER_TYPE_GAMEPAD (DEFAULT), CONTROLLER_TYPE_MULTI_AXIS
    gamepadConfig.setWhichAxes(true, true, true, true, true, true, true, true);	// Enable all axes
    gamepadConfig.setButtonCount(8);

    bleGamepad = new BleGamepad("ELRS Joystick", "ELRS", 100);
    bleGamepad->setTXPowerLevel(9);
    bleGamepad->begin(&gamepadConfig);

    handset->setRCDataCallback(BluetoothJoystickUpdateValues);
}

static bool initialize()
{
    registerButtonFunction(ACTION_BLE_JOYSTICK, [](){
        setConnectionState(bleJoystick);
    });
    return true;
}

static int timeout()
{
    BluetoothJoystickBegin();
    return DURATION_NEVER;
}

static int event()
{
    if (connectionState == bleJoystick) {
        hwTimer::stop();
        return 200;
    }
    return DURATION_NEVER;
}

device_t BLE_device = {
    .initialize = initialize,
    .start = nullptr,
    .event = event,
    .timeout = timeout,
    .subscribe = EVENT_CONNECTION_CHANGED,
};

#endif
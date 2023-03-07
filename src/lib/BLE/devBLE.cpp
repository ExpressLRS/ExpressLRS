#include "devBLE.h"

#if defined(PLATFORM_ESP32)

#include "common.h"
#include "CRSF.h"
#include "POWERMGNT.h"
#include "hwTimer.h"
#include "logging.h"

#include <BleGamepad.h>
#include <NimBLEDevice.h>

class ELRSGamepad : public BleGamepad {
    public:
        ELRSGamepad() : BleGamepad("ExpressLRS Joystick", "ELRS", 100) {};

    protected:
        void onStarted(NimBLEServer *pServer) {
            NimBLEDevice::setPower(ESP_PWR_LVL_P9);
        }
};

static ELRSGamepad *bleGamepad;

void BluetoothJoystickUpdateValues()
{
    if (bleGamepad->isConnected())
    {
        // map first 8 channels to axis
        int16_t data[8];
        for (uint8_t i = 0; i < 8; i++)
        {
            data[i] = map(ChannelData[i], CRSF_CHANNEL_VALUE_MIN, CRSF_CHANNEL_VALUE_MAX, 0, 32767);
        }
        bleGamepad->setAxes(data[0], data[1], data[4], data[5], data[2], data[3], data[6], data[7]);

        // map other 8 channels to buttons
        for (uint8_t i = 8; i < 16; i++)
        {
            if (ChannelData[i] >= CRSF_CHANNEL_VALUE_2000) {
                bleGamepad->press(i - 7);
            } else {
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
    if (bleGamepad != nullptr)
        return;

    // construct the BLE immediately to prevent reentry from events/timeout
    bleGamepad = new ELRSGamepad();

    POWERMGNT::setPower(MinPower);
    Radio.End();
    CRSF::RCdataCallback = BluetoothJoystickUpdateValues;

    BleGamepadConfiguration *gamepadConfig = new BleGamepadConfiguration();
    gamepadConfig->setAutoReport(false);

    DBGLN("Starting BLE Joystick!");
    bleGamepad->begin(gamepadConfig);
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
  .initialize = NULL,
  .start = NULL,
  .event = event,
  .timeout = timeout
};

#endif
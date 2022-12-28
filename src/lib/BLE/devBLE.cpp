#include "devBLE.h"

#if defined(PLATFORM_ESP32)

#include "common.h"
#include "CRSF.h"
#include "POWERMGNT.h"
#include "hwTimer.h"
#include "logging.h"

#include "devBLETele.h"

#include <BleGamepad.h>
#include <NimBLEDevice.h>

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
        int16_t data[8];

        for (uint8_t i = 0; i < 8; i++)
        {
            data[i] = map(CRSF::ChannelData[i], CRSF_CHANNEL_VALUE_MIN, CRSF_CHANNEL_VALUE_MAX, 0, 32767);
        }

        bleGamepad->setX(data[0]);
        bleGamepad->setY(data[1]);
        bleGamepad->setRX(data[2]);
        bleGamepad->setRY(data[3]);
        bleGamepad->setRudder(data[4]);
        bleGamepad->setThrottle(data[5]);
        bleGamepad->setSlider1(data[6]);
        bleGamepad->setSlider2(data[7]);

        bleGamepad->sendReport();
    }
}

void BluetoothJoystickBegin()
{
    // bleGamepad is null if it hasn't been started yet
    if (bleGamepad != nullptr)
        return;

    BluetoothTelemetryShutdown();

    NimBLEDevice::setOwnAddrType(BLE_OWN_ADDR_PUBLIC);

    // construct the BLE immediately to prevent reentry from events/timeout
    bleGamepad = new ELRSGamepad();

    hwTimer::updateInterval(10000);
    CRSF::setSyncParams(10000); // 100hz
    // CRSF::disableOpentxSync();
    POWERMGNT::setPower(MinPower);
    Radio.End();
    CRSF::RCdataCallback = BluetoothJoystickUpdateValues;

    BleGamepadConfiguration *gamepadConfig = new BleGamepadConfiguration();
    gamepadConfig->setAutoReport(false);
    gamepadConfig->setButtonCount(0);
    gamepadConfig->setHatSwitchCount(0);
    gamepadConfig->setWhichAxes(enableX, enableY, enableZ, enableRX, enableRY, enableRZ, enableSlider1, enableSlider2);
    gamepadConfig->setWhichSimulationControls(enableRudder, enableThrottle, enableAccelerator, enableBrake, enableSteering);

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
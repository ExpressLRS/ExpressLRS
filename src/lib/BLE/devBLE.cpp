#include "common.h"
#include "devBLE.h"

#if defined(PLATFORM_ESP32)

#if defined(Regulatory_Domain_AU_915) || defined(Regulatory_Domain_EU_868) || defined(Regulatory_Domain_IN_866) || defined(Regulatory_Domain_FCC_915) || defined(Regulatory_Domain_AU_433) || defined(Regulatory_Domain_EU_433)
extern SX127xDriver Radio;
#elif defined(Regulatory_Domain_ISM_2400)
extern SX1280Driver Radio;
#endif

#include "CRSF.h"
#include "POWERMGNT.h"
#include "hwTimer.h"
#include "logging.h"

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
            data[i] = map(CRSF::ChannelDataIn[i], CRSF_CHANNEL_VALUE_MIN - 1, CRSF_CHANNEL_VALUE_MAX + 1, -32768, 32768);
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

    // construct the BLE immediately to prevent reentry from events/timeout
<<<<<<< HEAD
    bleGamepad = new ELRSGamepad();
=======
    bleGamepad = new BleGamepad("ExpressLRS Joystick", "ELRS", 100);
    bleGamepad->setAutoReport(false);
    bleGamepad->setControllerType(CONTROLLER_TYPE_GAMEPAD);
>>>>>>> parent of 4fb6474b (Merge branch 'master' of https://github.com/SunjunKim/ExpressLRS)

    hwTimer::updateInterval(10000);
    CRSF::setSyncParams(10000); // 100hz
    // CRSF::disableOpentxSync();
    POWERMGNT::setPower(MinPower);
    Radio.End();
    CRSF::RCdataCallback = BluetoothJoystickUpdateValues;

    DBGLN("Starting BLE Joystick!");
    bleGamepad->begin(numOfButtons, numOfHatSwitches, enableX, enableY, enableZ, enableRZ, enableRX, enableRY, enableSlider1, enableSlider2, enableRudder, enableThrottle, enableAccelerator, enableBrake, enableSteering);
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
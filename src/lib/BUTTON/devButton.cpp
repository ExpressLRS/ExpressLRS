#include "targets.h"
#include "common.h"
#include "device.h"

#if defined(GPIO_PIN_BUTTON) && (GPIO_PIN_BUTTON != UNDEF_PIN)
#include "logging.h"
#include "button.h"

Button<GPIO_PIN_BUTTON, GPIO_BUTTON_INVERTED> button;

#if defined(TARGET_TX)
#include "POWERMGNT.h"
void EnterBindingMode();
#endif

static void initialize()
{
    #if defined(TARGET_TX_BETAFPV_2400_V1) || defined(TARGET_TX_BETAFPV_900_V1)
        button.OnShortPress = []() {
            if (button.getCount() == 3)
            {
                EnterBindingMode();
            }
        };
        button.OnLongPress = []() {
            // Only change power if we are running normally
            if (connectionState < MODE_STATES)
            {
                PowerLevels_e curr = POWERMGNT::currPower();
                if (curr == MaxPower)
                {
                    POWERMGNT::setPower(MinPower);
                }
                else
                {
                    POWERMGNT::incPower();
                }
                DBGLN("setpower %d", POWERMGNT::currPower());
                devicesTriggerEvent();
            }
        };
    #endif
    #if defined(TARGET_RX) && (defined(PLATFORM_ESP32) || defined(PLATFORM_ESP8266))
        button.OnLongPress = []() {
            if (button.getLongCount() > 4 && connectionState != wifiUpdate)
            {
                connectionState = wifiUpdate;
            }
            if (button.getLongCount() > 8)
            {
                ESP.restart();
            }
        };
    #endif
}

static int start()
{
    return DURATION_IMMEDIATELY;
}

static int timeout()
{
    return button.update();
}

device_t Button_device = {
    .initialize = initialize,
    .start = start,
    .event = NULL,
    .timeout = timeout
};

#else

device_t Button_device = {
    NULL
};

#endif
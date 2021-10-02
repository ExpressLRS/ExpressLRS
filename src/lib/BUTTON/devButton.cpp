#include "targets.h"
#include "common.h"
#include "device.h"

#if defined(GPIO_PIN_BUTTON) && (GPIO_PIN_BUTTON != UNDEF_PIN)
#include "logging.h"
#include "button.h"

Button<GPIO_PIN_BUTTON, GPIO_BUTTON_INVERTED> button;

#if defined(TARGET_TX_BETAFPV_2400_V1) || defined(TARGET_TX_BETAFPV_900_V1)
#include "POWERMGNT.h"
void EnterBindingMode();
#endif

static void initialize()
{
    #if defined(TARGET_TX_BETAFPV_2400_V1) || defined(TARGET_TX_BETAFPV_900_V1)
        button.OnShortPress = []() {
            if (button.getCount() == 3)
                EnterBindingMode();
        };
        button.OnLongPress = []() {
            switch (POWERMGNT::currPower())
            {
            case PWR_100mW:
                POWERMGNT::setPower(PWR_250mW);
                break;
            case PWR_250mW:
                POWERMGNT::setPower(PWR_500mW);
                break;
            case PWR_500mW:   
            default:
                POWERMGNT::setPower(PWR_100mW);
                break;
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
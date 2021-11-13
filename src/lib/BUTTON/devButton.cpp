#include "targets.h"
#include "common.h"
#include "device.h"

#if defined(GPIO_PIN_BUTTON) && (GPIO_PIN_BUTTON != UNDEF_PIN)
#include "logging.h"
#include "button.h"

static Button<GPIO_PIN_BUTTON, GPIO_BUTTON_INVERTED> button;

#if defined(TARGET_TX_BETAFPV_2400_V1) || defined(TARGET_TX_BETAFPV_900_V1) || defined(TARGET_TX_FLYSKY_FRM301)
#include "POWERMGNT.h"
void EnterBindingMode();

static void enterBindMode3Click()
{
    if (button.getCount() == 3)
    {
        EnterBindingMode();
    }
};

static void cyclePower()
{
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
        devicesTriggerEvent();
    }
};
#endif

#if defined(TARGET_RX) && (defined(PLATFORM_ESP32) || defined(PLATFORM_ESP8266))
static void rxWebUpdateReboot()
{
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

static void initialize()
{
    #if defined(TARGET_TX_BETAFPV_2400_V1) || defined(TARGET_TX_BETAFPV_900_V1)
        button.OnShortPress = enterBindMode3Click;
        button.OnLongPress = cyclePower;
    #endif
    #if defined(TARGET_RX) && (defined(PLATFORM_ESP32) || defined(PLATFORM_ESP8266))
        button.OnLongPress = rxWebUpdateReboot;
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

#endif
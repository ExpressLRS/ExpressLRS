#include "devButton.h"

#if defined(GPIO_PIN_BUTTON)
#include "common.h"
#include "logging.h"
#include "button.h"

static Button button;

#ifndef GPIO_BUTTON_INVERTED
#define GPIO_BUTTON_INVERTED false
#endif

// only check every second if the device is in-use, i.e. RX conencted, or TX is armed
static constexpr int MS_IN_USE = 1000;

#if defined(TARGET_TX)
#include "CRSF.h"
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
        if (curr == POWERMGNT::getMaxPower())
        {
            POWERMGNT::setPower(POWERMGNT::getMinPower());
        }
        else
        {
            POWERMGNT::incPower();
        }
        devicesTriggerEvent();
    }
};
#endif

#if defined(TARGET_RX)
#if defined(PLATFORM_ESP32)
#include <SPIFFS.h>
#elif defined(PLATFORM_ESP8266)
#include <FS.h>
#endif

extern void setWifiUpdateMode();

static void buttonRxLong()
{
#if defined(PLATFORM_ESP32) || defined(PLATFORM_ESP8266)
    // ESP/ESP32 goes to wifi mode in 5x longpress
    if (button.getLongCount() > 4 && connectionState != wifiUpdate)
    {
        setWifiUpdateMode();
    }
#endif

    // All RX reset their config in 9x longpress and reboot
    if (button.getLongCount() > 8)
    {
        config.SetDefaults(true);
#if defined(PLATFORM_ESP32) || defined(PLATFORM_ESP8266)
        // Prevent WDT from rebooting too early if
        // all this flash write is taking too long
        yield();
        // Remove options.json and hardware.json
        SPIFFS.format();
        ESP.restart();
#elif defined(PLATFORM_STM32)
        HAL_NVIC_SystemReset();
#endif
    }
}
#endif

static void initialize()
{
    if (GPIO_PIN_BUTTON != UNDEF_PIN)
    {
        button.init(GPIO_PIN_BUTTON, GPIO_BUTTON_INVERTED);
        #if defined(TARGET_TX)
            button.OnShortPress = enterBindMode3Click;
            button.OnLongPress = cyclePower;
        #endif
        #if defined(TARGET_RX)
            button.OnLongPress = buttonRxLong;
        #endif
    }
}

static int start()
{
    if (GPIO_PIN_BUTTON == UNDEF_PIN)
    {
        return DURATION_NEVER;
    }
    return DURATION_IMMEDIATELY;
}

static int timeout()
{
    if (GPIO_PIN_BUTTON == UNDEF_PIN)
    {
        return DURATION_NEVER;
    }
#if defined(TARGET_TX)
    if (CRSF::IsArmed())
        return MS_IN_USE;
#else
    if (connectionState == connected)
        return MS_IN_USE;
#endif

    return button.update();
}

device_t Button_device = {
    .initialize = initialize,
    .start = start,
    .event = NULL,
    .timeout = timeout
};

#endif
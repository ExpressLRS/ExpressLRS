#include "targets.h"
#include "common.h"
#include "device.h"

#if defined(GPIO_PIN_BUTTON) && (GPIO_PIN_BUTTON != UNDEF_PIN)
#include "logging.h"
#include "button.h"

#if defined(TARGET_TX)
Button<GPIO_PIN_BUTTON, GPIO_BUTTON_INVERTED> button;
#else
#define BUTTON_SAMPLE_INTERVAL 150
#define WEB_UPDATE_PRESS_INTERVAL 2000 // hold button for 2 sec to enable webupdate mode
#define BUTTON_RESET_INTERVAL 4000     //hold button for 4 sec to reboot RX

//// Variables Relating to Button behaviour ////
bool buttonPrevValue = true; //default pullup
bool buttonDown = false;     //is the button current pressed down?
uint32_t buttonLastSampled = 0;
uint32_t buttonLastPressed = 0;
#endif

#if defined(TARGET_TX_BETAFPV_2400_V1) || defined(TARGET_TX_BETAFPV_900_V1)
#include "POWERMGNT.h"
void EnterBindingMode();
#endif

static void initialize()
{
    #if defined(TARGET_TX_BETAFPV_2400_V1) || defined(TARGET_TX_BETAFPV_900_V1)
        button.OnShortPress = []() { if (button.getCount() == 3) EnterBindingMode(); };
        button.OnLongPress = &POWERMGNT::handleCyclePower;
    #endif
}

static int start()
{
    #if defined(TARGET_RX)
        pinMode(GPIO_PIN_BUTTON, INPUT);
    #endif
    return DURATION_IMMEDIATELY;
}

static int timeout()
{
    #if defined(TARGET_TX)
        button.update();
        return DURATION_IMMEDIATELY;
    #else
        bool buttonValue = digitalRead(GPIO_PIN_BUTTON);
        unsigned long now = millis();

        if (buttonValue == false && buttonPrevValue == true)
        { //falling edge
            buttonDown = true;
        }

        if (buttonValue == true && buttonPrevValue == false)
        { //rising edge
            buttonDown = false;
        }

        if ((now > buttonLastPressed + WEB_UPDATE_PRESS_INTERVAL) && buttonDown)
        { // button held down for WEB_UPDATE_PRESS_INTERVAL
    #if defined(PLATFORM_ESP32) || defined(PLATFORM_ESP8266)
            if (connectionState != wifiUpdate)
            {
                connectionState = wifiUpdate;
            }
    #endif
        }
        if ((now > buttonLastPressed + BUTTON_RESET_INTERVAL) && buttonDown)
        {
    #ifdef PLATFORM_ESP8266
            ESP.restart();
    #endif
        }
        buttonPrevValue = buttonValue;
        return BUTTON_SAMPLE_INTERVAL;
    #endif
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
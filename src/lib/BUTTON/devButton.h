#pragma once

#include "device.h"
#include "config.h"

typedef void (*ButtonAction_fn)();

// The config mode only allows a maximum of 2 actions per button
#define MAX_BUTTON_ACTIONS  2

// Limited to 16 possible ACTIONs by config storage currently
typedef enum : uint8_t {
    ACTION_NONE,
    ACTION_INCREASE_POWER,
    ACTION_GOTO_VTX_BAND,
    ACTION_GOTO_VTX_CHANNEL,
    ACTION_SEND_VTX,
    ACTION_START_WIFI,
    ACTION_BIND,
    ACTION_RESET_REBOOT,

    ACTION_LAST
} action_e;

#if defined(GPIO_PIN_BUTTON)
    #if defined(TARGET_TX) || \
        (defined(TARGET_RX) && (defined(PLATFORM_ESP32) || defined(PLATFORM_ESP8266)))
        extern device_t Button_device;
        #define HAS_BUTTON
    #endif

    #include <list>
    #include <map>

    typedef struct action {
        uint8_t button;
        bool longPress;
        uint8_t count;
        action_e action;
    } action_t;

    void registerButtonFunction(action_e action, ButtonAction_fn function);

#else
    inline void registerButtonFunction(uint8_t action, ButtonAction_fn function) {}
#endif

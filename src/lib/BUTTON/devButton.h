#pragma once

#include "device.h"
#include "common.h"

typedef void (*ButtonAction_fn)();

#if defined(GPIO_PIN_BUTTON)
    extern device_t Button_device;
    #define HAS_BUTTON

    #include <list>
    #include <map>

    typedef struct action {
        uint8_t button;
        bool longPress;
        uint8_t count;
        action_e action;
    } action_t;

    void registerButtonFunction(action_e action, ButtonAction_fn function);
    size_t button_GetActionCnt();
#else
    inline void registerButtonFunction(uint8_t action, ButtonAction_fn function) {}
    inline size_t button_GetActionCnt() { return 0; }
#endif

#pragma once

#include "device.h"
#include "common.h"

typedef void (*ButtonAction_fn)();

extern device_t Button_device;

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

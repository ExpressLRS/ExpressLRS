#include "devButton.h"

#include "logging.h"
#include "button.h"
#include "config.h"
#include "helpers.h"
#include "handset.h"

static Button button1;
static Button button2;

// only check every second if the device is in-use, i.e. RX connected, or TX is armed
static constexpr int MS_IN_USE = 1000;

#if defined(TARGET_RX)
static constexpr struct {
    bool pressType;
    uint8_t count;
    action_e action;
} button_actions[] = {
    // half second durations + 1 (i.e. 2=1.5s)
    {true, 2, ACTION_BIND},             // 1.5s
    {true, 9, ACTION_START_WIFI},       // 5.0s
    {true, 23, ACTION_RESET_REBOOT}     // 12.0s
};
#endif

static ButtonAction_fn actions[ACTION_LAST] = { nullptr };

void registerButtonFunction(action_e action, ButtonAction_fn function)
{
    actions[action] = function;
}

size_t button_GetActionCnt()
{
#if defined(TARGET_RX)
    return ARRAY_SIZE(button_actions);
#else
    return CONFIG_TX_BUTTON_ACTION_CNT;
#endif
}

static void handlePress(uint8_t button, bool longPress, uint8_t count)
{
    DBGLN("handlePress(%u, %u, %u)", button, (uint8_t)longPress, count);
#if defined(TARGET_TX)
    const button_action_t *button_actions = config.GetButtonActions(button)->val.actions;
#endif
    for (unsigned i=0 ; i<button_GetActionCnt() ; i++)
    {
        if (button_actions[i].action != ACTION_NONE && button_actions[i].pressType == longPress && button_actions[i].count == count-1)
        {
            if (actions[button_actions[i].action])
            {
                actions[button_actions[i].action]();
            }
        }
    }
}

static bool initialize()
{
    return GPIO_PIN_BUTTON != UNDEF_PIN || GPIO_PIN_BUTTON2 != UNDEF_PIN;
}

static int start()
{
    if (GPIO_PIN_BUTTON != UNDEF_PIN)
    {
        button1.init(GPIO_PIN_BUTTON);
        button1.OnShortPress = [](){ handlePress(0, false, button1.getCount()); };
        button1.OnLongPress = [](){ handlePress(0, true, button1.getLongCount()+1); };
    }
    if (GPIO_PIN_BUTTON2 != UNDEF_PIN)
    {
        button2.init(GPIO_PIN_BUTTON2);
        button2.OnShortPress = [](){ handlePress(1, false, button2.getCount()); };
        button2.OnLongPress = [](){ handlePress(1, true, button2.getLongCount()+1); };
    }

    return DURATION_IMMEDIATELY;
}

static int event()
{
#if defined(TARGET_TX)
    if (handset->IsArmed())
    {
        return DURATION_NEVER;
    }
#else
    if (connectionState == connected)
    {
        return DURATION_NEVER;
    }
#endif
    return DURATION_IMMEDIATELY;
}

static int timeout()
{
    int timeout = DURATION_NEVER;
    if (GPIO_PIN_BUTTON != UNDEF_PIN)
    {
        timeout = button1.update();
    }
    if (GPIO_PIN_BUTTON2 != UNDEF_PIN)
    {
        timeout = button2.update();
    }
    return timeout;
}

device_t Button_device = {
    .initialize = initialize,
    .start = start,
    .event = event,
    .timeout = timeout,
    .subscribe = EVENT_ARM_FLAG_CHANGED | EVENT_CONNECTION_CHANGED
};

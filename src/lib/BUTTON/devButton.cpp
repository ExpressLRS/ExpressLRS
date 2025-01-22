#include "devButton.h"

#if defined(GPIO_PIN_BUTTON)
#include "logging.h"
#include "button.h"
#include "config.h"
#include "helpers.h"
#include "handset.h"

#ifndef GPIO_BUTTON_INVERTED
#define GPIO_BUTTON_INVERTED false
#endif
#ifndef GPIO_BUTTON2_INVERTED
#define GPIO_BUTTON2_INVERTED false
#endif
#if !defined(GPIO_PIN_BUTTON2)
#define GPIO_PIN_BUTTON2 UNDEF_PIN
#endif

static Button button1;
#if defined(GPIO_PIN_BUTTON2)
static Button button2;
#endif

// only check every second if the device is in-use, i.e. RX connected, or TX is armed
static constexpr int MS_IN_USE = 1000;

#if defined(TARGET_RX)
static constexpr struct {
    bool pressType;
    uint8_t count;
    action_e action;
} button_actions[] = {
    {true, 2, ACTION_RESET_REBOOT}     // 12.0s
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

static int start()
{
    if (GPIO_PIN_BUTTON == UNDEF_PIN)
    {
        return DURATION_NEVER;
    }

    if (GPIO_PIN_BUTTON != UNDEF_PIN)
    {
        button1.init(GPIO_PIN_BUTTON, GPIO_BUTTON_INVERTED);
        button1.OnShortPress = [](){ handlePress(0, false, button1.getCount()); };
        button1.OnLongPress = [](){ handlePress(0, true, button1.getLongCount()+1); };
    }
#if defined(TARGET_TX)
    if (GPIO_PIN_BUTTON2 != UNDEF_PIN)
    {
        button2.init(GPIO_PIN_BUTTON2, GPIO_BUTTON_INVERTED);
        button2.OnShortPress = [](){ handlePress(1, false, button2.getCount()); };
        button2.OnLongPress = [](){ handlePress(1, true, button2.getLongCount()+1); };
    }
#endif

    return DURATION_IMMEDIATELY;
}

static int timeout()
{
    if (GPIO_PIN_BUTTON == UNDEF_PIN)
    {
        return DURATION_NEVER;
    }
#if defined(TARGET_TX)
    if (handset->IsArmed())
        return MS_IN_USE;
#else
    if (connectionState == connected)
        return MS_IN_USE;
#endif

#if defined(GPIO_PIN_BUTTON2)
    if (GPIO_PIN_BUTTON2 != UNDEF_PIN)
    {
        button2.update();
    }
#endif
    return button1.update();
}

device_t Button_device = {
    .initialize = nullptr,
    .start = start,
    .event = nullptr,
    .timeout = timeout
};

#endif

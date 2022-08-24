#include "devButton.h"

#if defined(GPIO_PIN_BUTTON)
#include "common.h"
#include "logging.h"
#include "button.h"
#include "config.h"
#include "devButton.h"

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

#if defined(TARGET_RX)
static constexpr struct {
    bool pressType;
    uint8_t count;
    action_e action;
} button_actions[2] = {
    {true, 3, ACTION_START_WIFI},
    {true, 7, ACTION_RESET_REBOOT}
};
#endif

static ButtonAction_fn actions[ACTION_LAST] = { nullptr };

void registerButtonFunction(action_e action, ButtonAction_fn function)
{
    actions[action] = function;
}

static void handlePress(uint8_t button, bool longPress, uint8_t count)
{
    DBGLN("handlePress(%u, %u, %u)", button, (uint8_t)longPress, count);
#if defined(TARGET_TX)
    const button_action_t *button_actions = config.GetButtonActions(button)->val.actions;
#endif
    for (int i=0 ; i<MAX_BUTTON_ACTIONS ; i++)
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
#if defined(TARGET_TX)
        const tx_button_color_t *button_actions = config.GetButtonActions(0);
        if (button_actions->val.actions[0].action == ACTION_NONE && button_actions->val.actions[1].action == ACTION_NONE)
        {
            // Set defaults for button 1
            tx_button_color_t default_actions = {
                .val = {
                    .color = 0,
                    .actions = {
                        {false, 2, ACTION_BIND},
                        {true, 0, ACTION_INCREASE_POWER}
                    }
                }
            };
            config.SetButtonActions(0, &default_actions);
        }
#endif
    }
    if (GPIO_PIN_BUTTON2 != UNDEF_PIN)
    {
        button2.init(GPIO_PIN_BUTTON2, GPIO_BUTTON_INVERTED);
        button2.OnShortPress = [](){ handlePress(1, false, button2.getCount()); };
        button2.OnLongPress = [](){ handlePress(1, true, button2.getLongCount()+1); };
#if defined(TARGET_TX)
        const tx_button_color_t *button_actions = config.GetButtonActions(1);
        if (button_actions->val.actions[0].action == ACTION_NONE && button_actions->val.actions[1].action == ACTION_NONE)
        {
            // Set defaults for button 2
            tx_button_color_t default_actions = {
                .val = {
                    .color = 0,
                    .actions = {
                        {false, 1, ACTION_GOTO_VTX_CHANNEL},
                        {true, 0, ACTION_SEND_VTX}
                    }
                }
            };
            config.SetButtonActions(1, &default_actions);
        }
#endif
    }

    return DURATION_IMMEDIATELY;
}

static int timeout()
{
    if (GPIO_PIN_BUTTON == UNDEF_PIN)
    {
        return DURATION_NEVER;
    }
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

#include "targets.h"

#if defined(PLATFORM_ESP32)

#include "devScreen.h"

#include "common.h"
#include "logging.h"

#include "OLED/oleddisplay.h"
#include "TFT/tftdisplay.h"

#include "devButton.h"
#include "handset.h"

FiniteStateMachine state_machine(entry_fsm);

Display *display;

#include "FiveWayButton/FiveWayButton.h"
FiveWayButton fivewaybutton;

#include "gsensor.h"
extern Gsensor gsensor;
static bool is_screen_flipped = false;
static bool is_pre_screen_flipped = false;

#define SCREEN_DURATION 20

extern void jumpToWifiRunning();
extern void jumpToBleRunning();

static bool jumpToBandSelect = false;
static bool jumpToChannelSelect = false;

static int handle(void)
{
    is_screen_flipped = gsensor.isFlipped();

    if ((is_screen_flipped == true) && (is_pre_screen_flipped == false))
    {
        display->doScreenBackLight(SCREEN_BACKLIGHT_OFF);
    }
    else if ((is_screen_flipped == false) && (is_pre_screen_flipped == true))
    {
        display->doScreenBackLight(SCREEN_BACKLIGHT_ON);
        state_machine.start(millis(), STATE_IDLE);
    }
    is_pre_screen_flipped = is_screen_flipped;
    if (is_screen_flipped)
    {
        return 100; // no need to check as often if the screen is off!
    }
    uint32_t now = millis();

    if (state_machine.getParentState() != STATE_WIFI_TX && connectionState == wifiUpdate)
    {
        jumpToWifiRunning();
    }
    
    if (state_machine.getParentState() != STATE_JOYSTICK && connectionState == bleJoystick)
    {
        jumpToBleRunning();
    }

    if (!handset->IsArmed())
    {
        int key;
        bool isLongPressed;
        // if we are using analog joystick then we can't cancel because WiFi is using the ADC2 (i.e. channel >= 8)!
        if (connectionState == wifiUpdate && digitalPinToAnalogChannel(GPIO_PIN_JOYSTICK) >= 8)
        {
            key = INPUT_KEY_NO_PRESS;
        }
        else
        {
            fivewaybutton.update(&key, &isLongPressed);
        }
        fsm_event_t fsm_event;
        switch (key)
        {
        case INPUT_KEY_DOWN_PRESS:
            fsm_event = EVENT_DOWN;
            break;
        case INPUT_KEY_UP_PRESS:
            fsm_event = EVENT_UP;
            break;
        case INPUT_KEY_LEFT_PRESS:
            fsm_event = EVENT_LEFT;
            break;
        case INPUT_KEY_RIGHT_PRESS:
            fsm_event = EVENT_RIGHT;
            break;
        case INPUT_KEY_OK_PRESS:
            fsm_event = EVENT_ENTER;
            break;
        default: // INPUT_KEY_NO_PRESS
            fsm_event = EVENT_TIMEOUT;
        }
        if (fsm_event != EVENT_TIMEOUT && isLongPressed)
        {
            fsm_event = (fsm_event | LONG_PRESSED);
        }
#if defined(DEBUG_SCREENSHOT)
        if (key == INPUT_KEY_DOWN_PRESS && isLongPressed)
        {
            DBGLN("state_%d", state_machine.getCurrentState());
            Display::printScreenshot();
        }
#endif
        if (jumpToBandSelect)
        {
            state_machine.jumpTo(vtx_menu_fsm, STATE_VTX_BAND);
            state_machine.jumpTo(value_select_fsm, STATE_VALUE_INIT);
            jumpToBandSelect = false;
        }
        else if (jumpToChannelSelect)
        {
            state_machine.jumpTo(vtx_menu_fsm, STATE_VTX_CHANNEL);
            state_machine.jumpTo(value_select_fsm, STATE_VALUE_INIT);
            jumpToChannelSelect = false;
        }
        else
        {
            state_machine.handleEvent(now, fsm_event);
        }
    }
    else
    {
        state_machine.handleEvent(now, EVENT_TIMEOUT);
    }

    return SCREEN_DURATION;
}

static void initialize()
{
    if (OPT_HAS_SCREEN)
    {
        fivewaybutton.init();
        if (OPT_HAS_TFT_SCREEN)
        {
            display = new TFTDisplay();
        }
        else
        {
            display = new OLEDDisplay();
        }
        display->init();
        state_machine.start(millis(), getInitialState());

        registerButtonFunction(ACTION_GOTO_VTX_BAND, [](){
            jumpToBandSelect = true;
            devicesTriggerEvent();
        });
        registerButtonFunction(ACTION_GOTO_VTX_CHANNEL, [](){
            jumpToChannelSelect = true;
            devicesTriggerEvent();
        });
    }
}

static int start()
{
    if (OPT_HAS_SCREEN)
    {
        return DURATION_IMMEDIATELY;
    }
    return DURATION_NEVER;
}

static int event()
{
    if (OPT_HAS_SCREEN)
    {
        return handle();
    }
    return DURATION_NEVER;
}

static int timeout()
{
    if (OPT_HAS_SCREEN)
    {
        return handle();
    }
    return DURATION_NEVER;
}

device_t Screen_device = {
    .initialize = initialize,
    .start = start,
    .event = event,
    .timeout = timeout};
#endif

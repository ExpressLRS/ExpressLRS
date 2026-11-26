#include "targets.h"

#if defined(PLATFORM_ESP32)  && !defined(PLATFORM_ESP32_C3)

#include "devScreen.h"

#include "common.h"
#include "logging.h"

#include "OLED/oleddisplay.h"
#include "TFT/tftdisplay.h"

#include "devButton.h"
#include "devADC.h"
#include "OTA.h"

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

#if defined(DEBUG_SCREEN_REMOTE)
/*
 * Bench remote control over the TX's USB serial port (debug builds only; the
 * USB CRSF/backpack input is disabled in tx_main so this owns the port):
 *   u d l r o  short press up/down/left/right/ok    U D L R O  long press
 *   !          dump the framebuffer                  #  cycle demo link state
 *   ~          toggle the demo generator
 */
extern Stream *TxUSB;
static int remoteKey = INPUT_KEY_NO_PRESS;
static bool remoteLong = false;

static uint32_t joyStreamUntil = 0;

static void pollRemote()
{
    // 'j': stream the raw joystick ADC (mV) for 60 s, to calibrate joystick_values
    static uint32_t lastJoy = 0;
    if ((int32_t)(joyStreamUntil - millis()) > 0 && GPIO_PIN_JOYSTICK != UNDEF_PIN && millis() - lastJoy >= 50)
    {
        lastJoy = millis();
        TxUSB->printf("JOY %u %d\n", (unsigned)lastJoy, getADCReading(ADC_JOYSTICK));
    }
    while (TxUSB && TxUSB->available())
    {
        const int c = TxUSB->read();
        const char *keys = "udlro";
        const int codes[] = {INPUT_KEY_UP_PRESS, INPUT_KEY_DOWN_PRESS, INPUT_KEY_LEFT_PRESS, INPUT_KEY_RIGHT_PRESS, INPUT_KEY_OK_PRESS};
        for (int i = 0; i < 5; i++)
        {
            if (c == keys[i] || c == keys[i] - 32)
            {
                remoteKey = codes[i];
                remoteLong = (c == keys[i] - 32);
            }
        }
        if (c == '!')
        {
            uiFreezeUntil = millis() + 1500; // hold the frame while the host re-requests rows
            display->dumpFrame(*TxUSB);
        }
        else if (c == '@')
        {
            // '@NNN' re-sends one row of the frame
            char d[4] = {0};
            const uint32_t t0 = millis();
            for (int i = 0; i < 3 && millis() - t0 < 50;)
            {
                const int ch = TxUSB->read();
                if (ch >= '0' && ch <= '9')
                    d[i++] = ch;
            }
            display->dumpFrame(*TxUSB, atoi(d), 1);
        }
        else if (c == 'j')
        {
            joyStreamUntil = millis() + 60000;
            TxUSB->print("JOYTABLE");
            for (unsigned i = 0; i < N_JOY_ADC_VALUES; i++)
                TxUSB->printf(" %u/%u", fivewaybutton.joyValue(i), fivewaybutton.fuzz(i));
            TxUSB->println();
        }
        else if (c == '?')
            TxUSB->printf("STATE %d parent %d up %u rst %d\n", state_machine.getCurrentState(), state_machine.getParentState(), (unsigned)millis(), (int)esp_reset_reason());
        else if (c == '#')
            uiDemoCycle();
        else if (c == '~')
            uiDemoSet(!ui.demo);
    }
}
#endif

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

    if (!isArmed)
    {
        int key = INPUT_KEY_NO_PRESS;
        bool isLongPressed = false;
        // if we are using analog joystick then we can't cancel because WiFi is using the ADC2 (i.e. channel >= 8)!
        if (connectionState == wifiUpdate && digitalPinToAnalogChannel(GPIO_PIN_JOYSTICK) >= 8)
        {
            key = INPUT_KEY_NO_PRESS;
        }
        else
        {
            fivewaybutton.update(&key, &isLongPressed);
        }
#if defined(DEBUG_SCREEN_REMOTE)
        pollRemote(); // also works in WiFi mode, where the analog joystick is ignored
        if (remoteKey != INPUT_KEY_NO_PRESS)
        {
            key = remoteKey;
            isLongPressed = remoteLong;
            remoteKey = INPUT_KEY_NO_PRESS;
        }
#endif
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
        // Side-to-side value picker: LEFT/RIGHT step the value, a long LEFT backs out
        if (state_machine.getCurrentState() == STATE_VALUE_SELECT && display->horizontalValuePicker())
        {
            if (fsm_event == EVENT_LEFT)
                fsm_event = EVENT_UP;   // previous value
            else if (fsm_event == EVENT_RIGHT)
                fsm_event = EVENT_DOWN; // next value
            else if (fsm_event == EVENT_LONG_LEFT)
                fsm_event = EVENT_LEFT; // back
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

    display->tick(now);

    return SCREEN_DURATION;
}

static bool initialize()
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
#if defined(DEBUG_SCREEN_REMOTE)
        uiDemoSet(true);
#endif
        state_machine.start(millis(), getInitialState());

        registerButtonFunction(ACTION_GOTO_VTX_BAND, [](){
            jumpToBandSelect = true;
            handle();
        });
        registerButtonFunction(ACTION_GOTO_VTX_CHANNEL, [](){
            jumpToChannelSelect = true;
            handle();
        });
    }
    return OPT_HAS_SCREEN;
}

static int start()
{
    return DURATION_IMMEDIATELY;
}

static int event()
{
    return handle();
}

static int timeout()
{
    return handle();
}

device_t Screen_device = {
    .initialize = initialize,
    .start = start,
    .event = event,
    .timeout = timeout,
    .subscribe = EVENT_CONNECTION_CHANGED | EVENT_ARM_FLAG_CHANGED
};
#endif

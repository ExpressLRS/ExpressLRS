#if defined(USE_OLED_SPI) || defined(USE_OLED_SPI_SMALL) || defined(USE_OLED_I2C) || defined(HAS_TFT_SCREEN)

#include "menu.h"

#include "common.h"
#include "device.h"
#include "logging.h"

FiniteStateMachine fsm(menu_fsm);

#include "display.h"
Display screen;

#ifdef HAS_FIVE_WAY_BUTTON
#include "FiveWayButton/FiveWayButton.h"
FiveWayButton fivewaybutton;
#endif

#ifdef HAS_GSENSOR
#include "gsensor.h"
extern Gsensor gsensor;
static bool is_screen_flipped = false;
static bool is_pre_screen_flipped = false;
#endif

#define SCREEN_DURATION 20

extern bool ICACHE_RAM_ATTR IsArmed();

#ifdef HAS_FIVE_WAY_BUTTON
static int handle(void)
{
#if defined(JOY_ADC_VALUES) && defined(PLATFORM_ESP32)
    // if we are using analog joystick then we can't cancel because WiFi is using the ADC2 (i.e. channel >= 8)!
    if (connectionState == wifiUpdate && digitalPinToAnalogChannel(GPIO_PIN_JOYSTICK) >= 8)
    {
        return DURATION_NEVER;
    }
#endif

#ifdef HAS_GSENSOR
    is_screen_flipped = gsensor.isFlipped();

    if ((is_screen_flipped == true) && (is_pre_screen_flipped == false))
    {
        screen.doScreenBackLight(SCREEN_BACKLIGHT_OFF);
    }
    else if ((is_screen_flipped == false) && (is_pre_screen_flipped == true))
    {
        screen.doScreenBackLight(SCREEN_BACKLIGHT_ON);
    }
    is_pre_screen_flipped = is_screen_flipped;
    if (is_screen_flipped)
    {
        return 100; // no need to check as often if the screen is off!
    }
#endif
    uint32_t now = millis();

    if (!IsArmed())
    {
        int key;
        bool isLongPressed;
        fivewaybutton.update(&key, &isLongPressed);
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
        fsm.handleEvent(now, fsm_event);
    }
    else
    {
        fsm.handleEvent(now, EVENT_TIMEOUT);
    }
    return SCREEN_DURATION;
}
#else
static int handle(void)
{
    return DURATION_NEVER;
}
#endif

static void initialize()
{
#ifdef HAS_FIVE_WAY_BUTTON
    fivewaybutton.init();
#endif
    screen.init();
    fsm.start(millis(), getInitialState());
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
    .timeout = timeout};
#endif
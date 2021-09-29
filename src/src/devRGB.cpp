#include "targets.h"
#include "common.h"
#include "device.h"

#include "LED.h"

#if WS2812_LED_IS_USED || (defined(PLATFORM_ESP32) && defined(GPIO_PIN_LED))

#include "crsf_protocol.h"
#include "POWERMGNT.h"

static enum {
    STARTUP = 0,
    NORMAL = 1
} blinkyState;

constexpr uint8_t LEDSEQ_RADIO_FAILED[] = {  10, 10 }; // 100ms on, 100ms off (fast blink)
constexpr uint8_t LEDSEQ_NO_CROSSFIRE[] = {  10, 90 }; // 100ms on, 900ms off (one blink/s)

#define NORMAL_UPDATE_INTERVAL 50

constexpr uint8_t rate_hue[RATE_MAX] =
{
    170,     // 500/200 hz  blue
    85,      // 250/100 hz  green
    21,      // 150/50 hz   orange
    0        // 50/25 hz    red
};

static blinkyColor_t blinkyColor;

static int blinkyUpdate() {
    static constexpr uint8_t hueStepValue = 1;
    static constexpr uint8_t lightnessStep = 5;

    WS281BsetLED(HsvToRgb(blinkyColor));
    if ((int)blinkyColor.h + hueStepValue > 255) {
        if ((int)blinkyColor.v - lightnessStep < 0) {
            blinkyState = NORMAL;
            return NORMAL_UPDATE_INTERVAL;
        }
        blinkyColor.v -= lightnessStep;
    } else {
        blinkyColor.h += hueStepValue;
    }
    return 3000/(256/hueStepValue);
}

static void initialize()
{
    WS281Binit();
    blinkyColor.h = 0;
    blinkyColor.s = 255;
    blinkyColor.v = 128;
}

static int start()
{
    blinkyState = STARTUP;
    #ifdef PLATFORM_ESP32
    // Only do the blinkies if it was NOT a software reboot
    if (esp_reset_reason() == ESP_RST_SW) {
        blinkyState = NORMAL;
        return NORMAL_UPDATE_INTERVAL;
    }
    #endif
    return DURATION_IMMEDIATELY;
}

static int timeout(std::function<void ()> sendSpam)
{
    if (blinkyState == STARTUP && connectionState < FAILURE_STATES)
    {
        return blinkyUpdate();
    }
    switch (connectionState)
    {
    case connected:
        // Set the color and we're done!
        blinkyColor.h = rate_hue[ExpressLRS_currAirRate_Modparams->index];
        blinkyColor.v = fmap(POWERMGNT::currPower(), 0, PWR_COUNT-1, 10, 128);
        WS281BsetLED(HsvToRgb(blinkyColor));
        return DURATION_NEVER;
    case tentative:
        // Set the color and we're done!
        blinkyColor.h = rate_hue[ExpressLRS_currAirRate_Modparams->index];
        blinkyColor.v = fmap(POWERMGNT::currPower(), 0, PWR_COUNT-1, 10, 50);
        WS281BsetLED(HsvToRgb(blinkyColor));
        return DURATION_NEVER;
    case disconnected:
        blinkyColor.h = rate_hue[ExpressLRS_currAirRate_Modparams->index];
        brightnessFadeLED(blinkyColor, 0, 64);
        return NORMAL_UPDATE_INTERVAL;
    case wifiUpdate:
        hueFadeLED(blinkyColor, 85, 85-30, 128, 2);      // Yellow->Green cross-fade
        return 5;
    case bleJoystick:
        hueFadeLED(blinkyColor, 170, 170+30, 128, 2);    // Blue cross-fade
        return 5;
    case radioFailed:
        blinkyColor.h = 0;
        return flashLED(blinkyColor, 192, 0, LEDSEQ_RADIO_FAILED, sizeof(LEDSEQ_RADIO_FAILED));
    case noCrossfire:
        blinkyColor.h = 10;
        return flashLED(blinkyColor, 192, 0, LEDSEQ_NO_CROSSFIRE, sizeof(LEDSEQ_NO_CROSSFIRE));
    default:
        return DURATION_NEVER;
    }
}

static int event(std::function<void ()> sendSpam)
{
    return timeout(sendSpam);
}

device_t RGB_device = {
    .initialize = initialize,
    .start = start,
    .event = event,
    .timeout = timeout
};

#else

device_t RGB_device = {
    NULL
};

#endif // WS2812_LED_IS_USED || (defined(PLATFORM_ESP32) && defined(GPIO_PIN_LED))

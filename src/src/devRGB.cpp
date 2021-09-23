#ifdef TARGET_TX

#include "targets.h"
#include "common.h"
#include "device.h"

#if WS2812_LED_IS_USED || (defined(PLATFORM_ESP32) && defined(GPIO_PIN_LED))

#include "crsf_protocol.h"
#include "POWERMGNT.h"
#include "LED.h"

static enum {
    STARTUP = 0,
    NORMAL = 1
} blinkyState;

constexpr uint8_t LEDSEQ_RADIO_FAILED[] = {  10, 10 }; // 100ms on, 100ms off (fast blink)
constexpr uint8_t LEDSEQ_NO_CROSSFIRE[] = {  10, 90 }; // 100ms on, 900ms off (one blink/s)

static int8_t lastRate = 0xFF;
static PowerLevels_e lastPower = PWR_COUNT;
static connectionState_e lastState = disconnected;

static uint32_t LEDupdateCounterMillis;
static uint32_t LEDupdateInterval;
#define NORMAL_UPDATE_INTERVAL 50

static blinkyColor_t blinkyColor;

static int blinkyUpdate(unsigned long now) {
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

static void initializeRGB()
{
    WS281Binit();
    blinkyState = STARTUP;
    #ifdef PLATFORM_ESP32
    // Only do the blinkies if it was NOT a software reboot
    if (esp_reset_reason() == ESP_RST_SW) {
        blinkyState = NORMAL;
        LEDupdateInterval = NORMAL_UPDATE_INTERVAL;
    }
    #endif
    blinkyColor.h = 0;
    blinkyColor.s = 255;
    blinkyColor.v = 128;

    LEDupdateCounterMillis = 0;
    LEDupdateInterval = 0;
}

static bool updateRGB(bool eventFired, unsigned long now)
{
    if (blinkyState == STARTUP && connectionState < FAILURE_STATES)
    {
        if (now - LEDupdateCounterMillis > LEDupdateInterval) {
            LEDupdateInterval = blinkyUpdate(now);
            LEDupdateCounterMillis = now;
        }
        return false;
    }

    constexpr uint8_t rate_hue[RATE_MAX] =
    {
        170,     // 500/200 hz  blue
        85,      // 250/100 hz  green
        21,      // 150/50 hz   orange
        0        // 50/25 hz    red
    };

    if (now - LEDupdateCounterMillis > LEDupdateInterval) {
        switch (connectionState)
        {
        case disconnected:
            blinkyColor.h = rate_hue[ExpressLRS_currAirRate_Modparams->index];
            brightnessFadeLED(blinkyColor, 0, 64);
            LEDupdateInterval = NORMAL_UPDATE_INTERVAL;
            break;
        case wifiUpdate:
            LEDupdateInterval = 5;
            hueFadeLED(blinkyColor, 85, 85-30, 128, 2);      // Yellow->Green cross-fade
            break;
        case bleJoystick:
            LEDupdateInterval = 5;
            hueFadeLED(blinkyColor, 170, 170+30, 128, 2);    // Blue cross-fade
            break;
        case radioFailed:
            blinkyColor.h = 0;
            LEDupdateInterval = flashLED(blinkyColor, 192, 0, LEDSEQ_RADIO_FAILED, sizeof(LEDSEQ_RADIO_FAILED));
            break;
        case noCrossfire:
            blinkyColor.h = 0;
            LEDupdateInterval = flashLED(blinkyColor, 192, 0, LEDSEQ_NO_CROSSFIRE, sizeof(LEDSEQ_NO_CROSSFIRE));
            break;
        default:
            break;
        }
        lastState = connectionState;
        LEDupdateCounterMillis = now;
        return false;
    }

    if (connectionState < MODE_STATES)
    {
        // 'connected' state LED updates, change color if rate and/or tx power change
        PowerLevels_e power = POWERMGNT::currPower();
        int8_t rate = ExpressLRS_currAirRate_Modparams->index;
        if (((connectionState != disconnected) && (lastState == disconnected)) || lastRate != rate || lastPower != power)
        {
            lastState = connectionState;
            lastPower = power;
            if (rate != lastRate)
            {
                blinkyColor.h = rate_hue[rate];
                lastRate = rate;
            }
            blinkyColor.v = fmap(power, 0, PWR_COUNT-1, 10, 128);
            WS281BsetLED(HsvToRgb(blinkyColor));
            LEDupdateInterval = 1000;
        }
    }
    return false;
}
#else
static void initializeRGB()
{
}

static bool updateRGB(bool eventFired, unsigned long now)
{
}
#endif

device_t RGB_device = {
    initializeRGB,
    updateRGB
};

#endif
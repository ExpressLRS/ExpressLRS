#include "targets.h"
#include "common.h"
#include "device.h"

#if defined(PLATFORM_ESP32) && defined(GPIO_PIN_LED_WS2812) && (GPIO_PIN_LED_WS2812 != UNDEF_PIN)
#include <NeoPixelBus.h>
static constexpr uint16_t PixelCount = 2;
#ifdef WS2812_IS_GRB
static NeoPixelBus<NeoGrbFeature, Neo800KbpsMethod> strip(PixelCount, GPIO_PIN_LED_WS2812);
#else
static NeoPixelBus<NeoRgbFeature, Neo800KbpsMethod> strip(PixelCount, GPIO_PIN_LED_WS2812);
#endif

void WS281Binit()
{
    strip.Begin();
}

void WS281BsetLED(uint32_t color)
{
    strip.ClearTo(RgbColor(HtmlColor(color)));
    strip.Show();
}
#endif

#if defined(PLATFORM_STM32) && (GPIO_PIN_LED_WS2812 != UNDEF_PIN) && (GPIO_PIN_LED_WS2812_FAST != UNDEF_PIN)
#include "STM32F3_WS2812B_LED.h"
#endif


#if defined(GPIO_PIN_LED_WS2812) && (GPIO_PIN_LED_WS2812 != UNDEF_PIN)

#include "logging.h"
#include "crsf_protocol.h"
#include "POWERMGNT.h"

typedef struct {
  uint8_t h, s, v;
} blinkyColor_t;

uint32_t HsvToRgb(blinkyColor_t &blinkyColor)
{
    uint8_t region, remainder, p, q, t;

    if (blinkyColor.s == 0)
    {
        return blinkyColor.v << 16 | blinkyColor.v << 8 | blinkyColor.v;
    }

    region = blinkyColor.h / 43;
    remainder = (blinkyColor.h - (region * 43)) * 6; 

    p = (blinkyColor.v * (255 - blinkyColor.s)) >> 8;
    q = (blinkyColor.v * (255 - ((blinkyColor.s * remainder) >> 8))) >> 8;
    t = (blinkyColor.v * (255 - ((blinkyColor.s * (255 - remainder)) >> 8))) >> 8;

    switch (region)
    {
        case 0:
            return blinkyColor.v << 16 | t << 8 | p;
        case 1:
            return q << 16 | blinkyColor.v << 8 | p;
        case 2:
            return p << 16 | blinkyColor.v << 8 | t;
        case 3:
            return p << 16 | q << 8 | blinkyColor.v;
        case 4:
            return t << 16 | p << 8 | blinkyColor.v;
        default:
            return blinkyColor.v << 16 | p << 8 | q;
    }
}

void brightnessFadeLED(blinkyColor_t &blinkyColor, uint8_t start, uint8_t end)
{
    static uint8_t lightness = 0;
    static int8_t dir = 1;

    if (lightness <= start)
    {
        lightness = start;
        dir = 1;
    }
    else if (lightness >= end)
    {
        lightness = end;
        dir = -1;
    }

    lightness += dir;

    blinkyColor.v = lightness;
    WS281BsetLED(HsvToRgb(blinkyColor));
}

void hueFadeLED(blinkyColor_t &blinkyColor, uint16_t start, uint16_t end, uint8_t lightness, uint8_t count)
{
    static bool hueMode = true;
    static uint16_t hue = 0;
    static int8_t dir = 1;
    static uint8_t counter = 0;

    if (!hueMode)
    {
        blinkyColor.v--;
        if (blinkyColor.v == 0)
        {
            hueMode = true;
        }
        WS281BsetLED(HsvToRgb(blinkyColor));
    }
    else
    {
        if (start < end)
        {
            if (hue <= start)
            {
                hue = start;
                dir = 1;
            }
            else if (hue >= end)
            {
                hue = end;
                dir = -1;
            }
        }
        else
        {
            if (hue >= start)
            {
                hue = start;
                dir = -1;
            }
            else if (hue <= end)
            {
                hue = end;
                dir = 1;
            }
        }

        blinkyColor.h = hue % 256;
        blinkyColor.v = lightness;
        WS281BsetLED(HsvToRgb(blinkyColor));
        hue += dir;
        if (count != 0 && hue == start)
        {
            counter++;
            if (counter >= count)
            {
                counter = 0;
                hueMode = false;
            }
        }
    }
}

uint16_t flashLED(blinkyColor_t &blinkyColor, uint8_t onLightness, uint8_t offLightness, const uint8_t durations[], uint8_t durationCounts)
{
    static int counter = 0;

    blinkyColor.v = counter % 2 == 0 ? onLightness : offLightness;
    WS281BsetLED(HsvToRgb(blinkyColor));
    if (counter >= durationCounts)
    {
        counter = 0;
    }
    return durations[counter++] * 10;
}

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

static int timeout()
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

device_t RGB_device = {
    .initialize = initialize,
    .start = start,
    .event = timeout,
    .timeout = timeout
};

#else

device_t RGB_device = {
    NULL
};

#endif

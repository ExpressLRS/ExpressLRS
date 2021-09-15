#if defined(PLATFORM_ESP32) && defined(GPIO_PIN_LED)
#include <NeoPixelBus.h>
static constexpr uint16_t PixelCount = 2;
#ifdef WS2812_IS_GRB
static NeoPixelBus<NeoGrbFeature, Neo800KbpsMethod> strip(PixelCount, GPIO_PIN_LED);
#else
static NeoPixelBus<NeoRgbFeature, Neo800KbpsMethod> strip(PixelCount, GPIO_PIN_LED);
#endif
#endif

#if (GPIO_PIN_LED_WS2812 != UNDEF_PIN) && (GPIO_PIN_LED_WS2812_FAST != UNDEF_PIN)
#include "STM32F3_WS2812B_LED.h"
#endif

#if defined(PLATFORM_ESP32) && defined(GPIO_PIN_LED)
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

#if WS2812_LED_IS_USED || (defined(PLATFORM_ESP32) && defined(GPIO_PIN_LED))
static enum {
    STARTUP = 0,
    NORMAL = 1
} blinkyState;
static struct blinkColor_t {
  uint8_t h, s, v;
} blinkyColor;

static uint32_t LEDupdateCounterMillis;
static uint32_t LEDupdateInterval;
#define NORMAL_UPDATE_INTERVAL 50

static uint32_t HsvToRgb()
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

static int blinkyUpdate(unsigned long now) {
    static constexpr uint8_t hueStepValue = 1;
    static constexpr uint8_t lightnessStep = 5;

    uint32_t rgb = HsvToRgb();
    WS281BsetLED(rgb);
    DBGVLN("LED hue %u", blinkyColor.h);

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

void startupLEDs()
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

static void brightnessFadeLED(uint8_t hue, uint8_t start, uint8_t end)
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

    blinkyColor.h = hue;
    blinkyColor.v = lightness;
    WS281BsetLED(HsvToRgb());
}

static void hueFadeLED(uint8_t lightness, uint8_t start, uint8_t end)
{
    static uint8_t hue = 142;
    static int8_t dir = 1;

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

    hue += dir;

    blinkyColor.h = hue;
    blinkyColor.v = lightness;
    WS281BsetLED(HsvToRgb());
}

void updateLEDs(uint32_t now, connectionState_e connectionState, uint8_t rate, uint32_t power)
{
    if (blinkyState == STARTUP)
    {
        if (now - LEDupdateCounterMillis > LEDupdateInterval) {
            LEDupdateInterval = blinkyUpdate(now);
            LEDupdateCounterMillis = now;
        }
        return;
    }

    constexpr uint8_t rate_hue[RATE_MAX] =
    {
        170,     // 500/200 hz  blue
        85,      // 250/100 hz  green
        21,      // 150/50 hz   orange
        0        // 50/25 hz    red
    };

    static connectionState_e lastState = disconnected;

    if (now - LEDupdateCounterMillis > LEDupdateInterval) {
        if (connectionState == disconnected)
        {
            brightnessFadeLED(rate_hue[rate], 0, 64);
            LEDupdateInterval = NORMAL_UPDATE_INTERVAL;
        }
        else if (connectionState == wifiUpdate)
        {
            hueFadeLED(128, 85-15, 85+15); // Green cross-fade
            LEDupdateInterval = 10;
        }
        else if (connectionState == bleJoystick)
        {
            hueFadeLED(128, 170-15, 170+15); // Blue cross-fade
            LEDupdateInterval = 10;
        }
        lastState = connectionState;
        LEDupdateCounterMillis = now;
    }

    // 'connected' state LED updates, change color if rate and/or tx power change
    static uint8_t lastRate = 0xFF;
    static uint32_t lastPower = 0xFFFFFFFF;
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
        WS281BsetLED(HsvToRgb());
        LEDupdateInterval = 1000;
    }
}
#else
void startupLEDs()
{
}

void updateLEDs(uint32_t now, connectionState_e connectionState, uint8_t rate, uint32_t power)
{
}
#endif

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
} state;
static unsigned long blinkyUpdateTime;
static uint8_t hue = 0, saturation = 255, lightness = 128;
static uint8_t hueStepValue = 1;
static uint8_t lightnessStep = 5;

static uint32_t HsvToRgb(uint8_t hue, uint8_t saturation, uint8_t lightness)
{
    uint8_t region, remainder, p, q, t;

    if (saturation == 0)
    {
        return lightness << 16 | lightness << 8 | lightness;
    }

    region = hue / 43;
    remainder = (hue - (region * 43)) * 6; 

    p = (lightness * (255 - saturation)) >> 8;
    q = (lightness * (255 - ((saturation * remainder) >> 8))) >> 8;
    t = (lightness * (255 - ((saturation * (255 - remainder)) >> 8))) >> 8;

    switch (region)
    {
        case 0:
            return lightness << 16 | t << 8 | p;
        case 1:
            return q << 16 | lightness << 8 | p;
        case 2:
            return p << 16 | lightness << 8 | t;
        case 3:
            return p << 16 | q << 8 | lightness;
        case 4:
            return t << 16 | p << 8 | lightness;
        default:
            return lightness << 16 | p << 8 | q;
    }
}

static void blinkyUpdate(unsigned long now) {
    if (blinkyUpdateTime > now)
    {
        return;
    }

    uint32_t rgb = HsvToRgb(hue, saturation, lightness);
    WS281BsetLED(rgb);
    DBGVLN("LED hue %u", hue);
    if ((int)hue + hueStepValue > 255) {
        if ((int)lightness - lightnessStep < 0) {
            state = NORMAL;
            return;
        }
        lightness -= lightnessStep;
    } else {
        hue += hueStepValue;
    }
    blinkyUpdateTime = now + 3000/(256/hueStepValue);
}

void startupLEDs()
{
    WS281Binit();
    state = STARTUP;
    #ifdef PLATFORM_ESP32
    // Only do the blinkies if it was NOT a software reboot
    if (esp_reset_reason() == ESP_RST_SW) {
        state = NORMAL;
    }
    #endif
    blinkyUpdateTime = 0;
    hue = 0;
    lightness = 128;
}

void updateLEDs(uint32_t now, connectionState_e connectionState, uint8_t rate, uint32_t power)
{
    if (state == STARTUP)
    {
        blinkyUpdate(now);
        return;
    }

    constexpr uint8_t rate_hue[RATE_MAX] =
    {
        170,     // 500/200 hz  blue
        85,     // 250/100 hz  green
        21,     // 150/50 hz   orange
        0      // 50/25 hz    red
    };
    constexpr uint32_t LEDupdateInterval = 50;
    static uint32_t nextUpdateTime = 0;

    static connectionState_e lastState = disconnected;
    static uint8_t lastRate = 0xFF;
    static uint32_t lastPower = 0xFFFFFFFF;

    if ((connectionState == disconnected) && (now > nextUpdateTime))
    {
        static uint8_t lightness = 0;
        static int8_t dir = 1;

        if (lightness == 64)
        {
            dir = -1;
        }
        else if (lightness == 0)
        {
            dir = 1;
        }

        lightness += dir;
        nextUpdateTime = now + LEDupdateInterval;
        lastState = connectionState;
        WS281BsetLED(HsvToRgb(rate_hue[rate], 255, lightness));
    }
    if (((connectionState != disconnected) && (lastState == disconnected)) || lastRate != rate || lastPower != power)
    {
        lastState = connectionState;
        lastRate = rate;
        lastPower = power;
        uint8_t lightness = fmap(power, 0, PWR_COUNT-1, 10, 128);
        WS281BsetLED(HsvToRgb(rate_hue[rate], 255, lightness));
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

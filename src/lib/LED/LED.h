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

void WS281BsetLED(uint8_t const r, uint8_t const g, uint8_t const b) // takes RGB data
{
    strip.ClearTo(RgbColor(r, g, b));
    strip.Show();
}
#endif

#if WS2812_LED_IS_USED || (defined(PLATFORM_ESP32) && defined(GPIO_PIN_LED))
static unsigned long blinkyUpdateTime;
static enum {
    STARTUP = 0,
    NORMAL = 1
} state;
static uint8_t hue = 0, saturation = 200, lightness = 255;
static uint8_t hueStepValue = 1;
static uint8_t lightnessStep = 10;

static RgbColor HsvToRgb(uint8_t hue, uint8_t saturation, uint8_t lightness)
{
    uint8_t region, remainder, p, q, t;

    if (saturation == 0)
    {
        return RgbColor(lightness, lightness, lightness);
    }

    region = hue / 43;
    remainder = (hue - (region * 43)) * 6; 

    p = (lightness * (255 - saturation)) >> 8;
    q = (lightness * (255 - ((saturation * remainder) >> 8))) >> 8;
    t = (lightness * (255 - ((saturation * (255 - remainder)) >> 8))) >> 8;

    switch (region)
    {
        case 0:
            return RgbColor(lightness, t, p);
        case 1:
            return RgbColor(q, lightness, p);
        case 2:
            return RgbColor(p, lightness, t);
        case 3:
            return RgbColor(p, q, lightness);
        case 4:
            return RgbColor(t, p, lightness);
        default:
            return RgbColor(lightness, p, q);
    }
}

static void blinkyUpdate() {
    unsigned long now = millis();
    if (blinkyUpdateTime > now)
    {
        return;
    }

    RgbColor rgb = HsvToRgb(hue, saturation, lightness);
    WS281BsetLED(rgb.R, rgb.G, rgb.B);
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
    lightness = 255;
}

void updateLEDs(uint32_t now, connectionState_e connectionState, uint8_t rate, uint32_t power)
{
    if (state == STARTUP)
    {
        blinkyUpdate();
        return;
    }
    constexpr uint32_t rate_colors[RATE_MAX] =
    {
        0x0000FF,     // 500/200 hz  blue
        0x00FF00,     // 250/100 hz  green
        0xFF8000,     // 150/50 hz   orange
        0xFF0000      // 50/25 hz    red
    };
    constexpr uint32_t LEDupdateInterval = 100;
    static uint8_t LEDfade = 0;
    static int8_t LEDfadeAmount = 2;
    static uint32_t LEDupdateCounterMillis;

    static connectionState_e lastState = disconnected;
    static uint32_t lastColor = 0xFFFFFFFF;
    static uint32_t lastPower = 0xFFFFFFFF;

    uint32_t color = rate_colors[rate];
    if ((connectionState == disconnected) && (now - LEDupdateCounterMillis > LEDupdateInterval))
    {
        if (LEDfade == 30)
        {
            LEDfadeAmount = -2;
        }
        else if (LEDfade == 0)
        {
            LEDfadeAmount = 2;
        }

        LEDfade += LEDfadeAmount;
        LEDupdateCounterMillis = now;
        lastState = connectionState;
        WS281BsetLED(
            (color >> 16) * LEDfade / 255,
            ((color >> 8) & 0xFF) * LEDfade / 255,
            (color & 0xFF) * LEDfade / 255
        );
    }
    if (((connectionState != disconnected) && (lastState == disconnected)) || lastColor != color || lastPower != power)
    {
        lastState = connectionState;
        lastColor = color;
        lastPower = power;
        int dim = fmap(power, 0, PWR_COUNT-1, 10, 255);
        WS281BsetLED(
            (color >> 16) * dim / 255,
            ((color >> 8) & 0xFF) * dim / 255,
            (color & 0xFF) * dim / 255
        );
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

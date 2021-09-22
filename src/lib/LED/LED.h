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

#include "logging.h"

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
typedef struct {
  uint8_t h, s, v;
} blinkyColor_t;

static uint32_t HsvToRgb(blinkyColor_t &blinkyColor)
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

static void hueFadeLED(blinkyColor_t &blinkyColor, uint16_t start, uint16_t end, uint8_t lightness, uint8_t count)
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

static uint16_t flashLED(blinkyColor_t &blinkyColor, uint8_t onLightness, uint8_t offLightness, const uint8_t durations[], uint8_t durationCounts)
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

#endif
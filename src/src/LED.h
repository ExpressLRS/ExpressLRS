/// LED SUPPORT ///////
uint8_t LEDfade=0;
int8_t LEDfadeAmount = 2;
constexpr uint32_t LEDupdateInterval = 100;
uint32_t LEDupdateCounterMillis;

static connectionState_e lastState = disconnected;
static uint32_t lastColor = 0xFFFFFFFF;
static uint32_t lastPower = 0xFFFFFFFF;

#if defined(PLATFORM_ESP32) && defined(GPIO_PIN_LED)
#include <NeoPixelBus.h>
static const uint16_t PixelCount = 2; // this example assumes 2 pixel
#ifdef WS2812_IS_GRB
static NeoPixelBus<NeoGrbFeature, Neo800KbpsMethod> strip(PixelCount, GPIO_PIN_LED);
#else
static NeoPixelBus<NeoRgbFeature, Neo800KbpsMethod> strip(PixelCount, GPIO_PIN_LED);
#endif
#endif

#if (GPIO_PIN_LED_WS2812 != UNDEF_PIN) && (GPIO_PIN_LED_WS2812_FAST != UNDEF_PIN)
#include "STM32F3_WS2812B_LED.h"
#endif

static uint32_t colors[8] = {
    0xFFFFFF,     // white
    0xFF00FF,     // magenta
    0x8000FF,     // violet
    0x0000FF,     // blue
    0x00FF00,     // green
    0xFFFF00,     // yellow
    0xFF8000,     // orange
    0xFF0000      // red
};

static uint32_t rate_colors[RATE_MAX] = {
    0x00FF00,     // 500/250/200 hz  green
    0xFFFF00,     // 250/150/100 hz  yellow
    0xFF8000,     // 150/50/50 hz    orange
    0xFF0000      // 50/25/25 hz     red
};

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

void updateLEDs(uint32_t now, connectionState_e connectionState, uint8_t rate, uint32_t power)
{
#if (defined(PLATFORM_ESP32) && defined(GPIO_PIN_LED)) || defined(WS2812_LED_IS_USED)
    uint32_t color = rate_colors[rate];
    if ((connectionState == disconnected) && (now > (LEDupdateCounterMillis + LEDupdateInterval)))
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
            (color >> 16) * LEDfade / 256,
            ((color >> 8) & 0xFF) * LEDfade / 256,
            (color & 0xFF) * LEDfade / 256
        );
    }
    if (((connectionState != disconnected) && (lastState == disconnected)) || lastColor != color || lastPower != power)
    {
        lastState = connectionState;
        lastColor = color;
        lastPower = power;
        int dim = fmap(power, 0, PWR_COUNT-1, 10, 255);
        WS281BsetLED(
            (color >> 16) * dim / 256,
            ((color >> 8) & 0xFF) * dim / 256,
            (color & 0xFF) * dim / 256
        );
    }
#endif
}

void startupLEDs()
{
#if WS2812_LED_IS_USED || (defined(PLATFORM_ESP32) && defined(GPIO_PIN_LED))
    // do startup blinkies for fun
    WS281Binit();
    for (uint8_t i = 0; i < RATE_ENUM_MAX; i++)
    {
        WS281BsetLED(colors[i]);
        delay(100);
    }
    for (uint8_t i = 0; i < RATE_ENUM_MAX; i++)
    {
        WS281BsetLED(colors[RATE_ENUM_MAX-i-1]);
        delay(100);
    }
    #if WS2812_LED_IS_USED
    WS281BsetLED(0, 0, 0);
    #else
    WS281BsetLED(0);
    #endif
#endif
}
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

    WS281BsetLED(HsvToRgb());
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

static uint16_t hueFadeLED(uint8_t lightness, uint16_t start, uint16_t end, uint16_t baseTime, uint8_t count)
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
        WS281BsetLED(HsvToRgb());
        return baseTime;
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
        WS281BsetLED(HsvToRgb());
        hue += dir;
        if (hue == start) {
            counter++;
        }
        if (count != 0 && counter == count)
        {
            counter = 0;
            hueMode = false;
        }
    }
    return baseTime;
}

static uint16_t flashLED(uint8_t onLightness, uint8_t offLightness, uint8_t hue, const uint8_t durations[], uint8_t durationCounts)
{
    static int counter = 0;

    blinkyColor.v = counter % 2 == 0 ? onLightness : offLightness;
    blinkyColor.h = hue;
    WS281BsetLED(HsvToRgb());
    if (counter == durationCounts)
    {
        counter = 0;
    }
    return durations[counter++] * 10;
}

constexpr uint8_t LEDSEQ_RADIO_FAILED[] = {  10, 10 }; // 100ms on, 100ms off (fast blink)
constexpr uint8_t LEDSEQ_NO_CROSSFIRE[] = {  10, 90 }; // 100ms on, 900ms off (one blink/s)

void updateLEDs(uint32_t now, connectionState_e connectionState, uint8_t rate, uint32_t power)
{
    if (blinkyState == STARTUP && connectionState < FAILURE_STATES)
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
        switch (connectionState)
        {
        case disconnected:
            brightnessFadeLED(rate_hue[rate], 0, 64);
            LEDupdateInterval = NORMAL_UPDATE_INTERVAL;
            break;
        case wifiUpdate:
            LEDupdateInterval = hueFadeLED(128, 85, 85-30, 5, 2);      // Yellow->Green cross-fade
            break;
        case bleJoystick:
            LEDupdateInterval = hueFadeLED(128, 170, 170+30, 5, 2);    // Blue cross-fade
            break;
        case radioFailed:
            LEDupdateInterval = flashLED(192, 0, 0, LEDSEQ_RADIO_FAILED, sizeof(LEDSEQ_RADIO_FAILED));
            break;
        case noCrossfire:
            LEDupdateInterval = flashLED(192, 0, 0, LEDSEQ_NO_CROSSFIRE, sizeof(LEDSEQ_NO_CROSSFIRE));
            break;
        default:
            break;
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

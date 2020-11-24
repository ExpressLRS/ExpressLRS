#include <Arduino.h>
#include <stdint.h>
#include "targets.h"

#if (GPIO_PIN_LED_WS2812 != UNDEF_PIN) && (GPIO_PIN_LED_WS2812_FAST != UNDEF_PIN)
#define WS2812_LED_IS_USED 1

static inline void LEDsend_1(void) {
        digitalWriteFast(GPIO_PIN_LED_WS2812_FAST, HIGH);
        __NOP(); __NOP(); __NOP(); __NOP(); __NOP();
        __NOP(); __NOP(); __NOP(); __NOP(); __NOP();
        __NOP(); __NOP(); __NOP(); __NOP(); __NOP();
        __NOP(); __NOP(); __NOP(); __NOP(); __NOP();
        __NOP(); __NOP(); __NOP(); __NOP(); __NOP();
        __NOP(); __NOP(); __NOP(); __NOP(); __NOP();
        __NOP(); __NOP(); __NOP(); __NOP(); __NOP();
        __NOP(); __NOP(); __NOP(); __NOP(); __NOP();
        __NOP(); __NOP(); __NOP(); __NOP(); __NOP();
        __NOP(); __NOP(); __NOP();
        digitalWriteFast(GPIO_PIN_LED_WS2812_FAST, LOW);
        __NOP(); __NOP(); __NOP(); __NOP(); __NOP();
        __NOP(); __NOP(); __NOP(); __NOP(); __NOP();
        __NOP(); __NOP(); __NOP(); __NOP(); __NOP();
        __NOP(); __NOP(); __NOP(); __NOP();
}

static inline void LEDsend_0(void) {
        digitalWriteFast(GPIO_PIN_LED_WS2812_FAST, HIGH);
        __NOP(); __NOP(); __NOP(); __NOP(); __NOP();
        __NOP(); __NOP(); __NOP(); __NOP(); __NOP();
        __NOP(); __NOP(); __NOP(); __NOP(); __NOP();
        __NOP(); __NOP(); __NOP(); __NOP();
        digitalWriteFast(GPIO_PIN_LED_WS2812_FAST, LOW);
        __NOP(); __NOP(); __NOP(); __NOP(); __NOP();
        __NOP(); __NOP(); __NOP(); __NOP(); __NOP();
        __NOP(); __NOP(); __NOP(); __NOP(); __NOP();
        __NOP(); __NOP(); __NOP(); __NOP(); __NOP();
        __NOP(); __NOP(); __NOP(); __NOP(); __NOP();
        __NOP(); __NOP(); __NOP(); __NOP(); __NOP();
        __NOP(); __NOP(); __NOP(); __NOP(); __NOP();
        __NOP(); __NOP(); __NOP(); __NOP(); __NOP();
        __NOP(); __NOP(); __NOP(); __NOP(); __NOP();
        __NOP(); __NOP(); __NOP();
}

static inline uint32_t bitReverse(uint8_t input)
{
    uint8_t r = input; // r will be reversed bits of v; first get LSB of v
    uint8_t s = 8 - 1; // extra shift needed at end

    for (input >>= 1; input; input >>= 1)
    {
        r <<= 1;
        r |= input & 1;
        s--;
    }
    r <<= s; // shift when input's highest bits are zero
    return r;
}

void WS281BsetLED(uint8_t const * const RGB) // takes RGB data
{
    pinMode(GPIO_PIN_LED_WS2812, OUTPUT);

    uint32_t LedColourData =
        bitReverse(RGB[1]) +       // Green
        (bitReverse(RGB[0]) << 8) +  // Red
        (bitReverse(RGB[2]) << 16);  // Blue
    uint8_t bits = 24;
    while (bits--)
    {
        (LedColourData & 0x1) ? LEDsend_1() : LEDsend_0();
        LedColourData >>= 1;
    }
    delayMicroseconds(50); // needed to latch in the values
}

void WS281BsetLED(uint8_t const r, uint8_t const g, uint8_t const b) // takes RGB data
{
    uint8_t data[3] = {r, g, b};
    WS281BsetLED(data);
}

void WS281BsetLED(uint32_t color) // takes RGB data
{
    uint8_t data[3];
    data[0] = (color & 0x00FF0000) >> 16;
    data[1] = (color & 0x0000FF00) >> 8;
    data[2] = (color & 0x000000FF) >> 0;
    WS281BsetLED(data);
}

#endif /* (GPIO_PIN_LED_WS2812 != UNDEF_PIN) && (GPIO_PIN_LED_WS2812_FAST != UNDEF_PIN) */

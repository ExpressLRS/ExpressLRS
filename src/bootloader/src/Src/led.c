#include "led.h"
#include "main.h"

#if defined(WS2812_LED_PIN)
struct gpio_pin led_pin;

#ifndef BRIGHTNESS
#define BRIGHTNESS 30 // 1...256
#endif

#define WS2812_DELAY_LONG() \
    __NOP(); __NOP(); __NOP(); __NOP(); __NOP(); \
    __NOP(); __NOP(); __NOP(); __NOP(); __NOP(); \
    __NOP(); __NOP(); __NOP(); __NOP(); __NOP(); \
    __NOP(); __NOP(); __NOP(); __NOP(); __NOP(); \
    __NOP(); __NOP(); __NOP(); __NOP(); __NOP(); \
    __NOP(); __NOP(); __NOP(); __NOP(); __NOP(); \
    __NOP(); __NOP(); __NOP(); __NOP(); __NOP(); \
    __NOP(); __NOP(); __NOP(); __NOP(); __NOP(); \
    __NOP();

#define WS2812_DELAY_SHORT() \
    __NOP(); __NOP(); __NOP(); __NOP(); __NOP(); \
    __NOP(); __NOP(); __NOP(); __NOP(); __NOP(); \
    __NOP(); __NOP(); __NOP(); __NOP(); __NOP(); \
    __NOP();


static inline void
ws2812_send_1(struct gpio_pin pin)
{
    GPIO_Write(pin, 1);
    WS2812_DELAY_LONG();
    GPIO_Write(pin, 0);
    WS2812_DELAY_SHORT();
}

static inline void
ws2812_send_0(struct gpio_pin pin)
{
    GPIO_Write(pin, 1);
    WS2812_DELAY_SHORT();
    GPIO_Write(pin, 0);
    WS2812_DELAY_LONG();
}

static uint32_t bitReverse(uint32_t input)
{
    // r will be reversed bits of v; first get LSB of v
    uint8_t r = (uint8_t)((input * BRIGHTNESS) >> 8);
    uint8_t s = 8 - 1; // extra shift needed at end

    for (input >>= 1; input; input >>= 1) {
        r <<= 1;
        r |= input & 1;
        s--;
    }
    r <<= s; // shift when input's highest bits are zero
    return r;
}

static void ws2812_send_color(uint8_t const *const RGB) // takes RGB data
{
    struct gpio_pin pin = led_pin;
    uint32_t LedColourData =
        bitReverse(RGB[1]) +        // Green
        (bitReverse(RGB[0]) << 8) + // Red
        (bitReverse(RGB[2]) << 16); // Blue
    uint8_t bits = 24;
    while (bits--) {
        (LedColourData & 0x1) ? ws2812_send_1(pin) : ws2812_send_0(pin);
        LedColourData >>= 1;
    }
}

void ws2812_init(void)
{
    led_pin = GPIO_Setup(IO_CREATE(WS2812_LED_PIN), GPIO_OUTPUT, -1);
}

void ws2812_set_color(uint8_t const r, uint8_t const g, uint8_t const b)
{
    uint8_t data[3] = {r, g, b};
    ws2812_send_color(data);
}

void ws2812_set_color_u32(uint32_t const rgb)
{
    uint8_t data[3] = {(uint8_t)(rgb), (uint8_t)(rgb >> 8), (uint8_t)(rgb >> 16)};
    ws2812_send_color(data);
}
#endif /* WS2812_RED */

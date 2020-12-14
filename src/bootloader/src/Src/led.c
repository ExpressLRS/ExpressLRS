#include "led.h"
#include "main.h"

#if defined(WS2812_LED_PIN)
static void *ws2812_port;
static uint32_t ws2812_pin;


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
ws2812_send_1(void * const port, uint32_t const pin)
{
    GPIO_WritePin(port, pin, 1);
    WS2812_DELAY_LONG();
    GPIO_WritePin(port, pin, 0);
    WS2812_DELAY_SHORT();
}

static inline void
ws2812_send_0(void * const port, uint32_t const pin)
{
    GPIO_WritePin(port, pin, 1);
    WS2812_DELAY_SHORT();
    GPIO_WritePin(port, pin, 0);
    WS2812_DELAY_LONG();
}

static uint32_t bitReverse(uint8_t input)
{
    uint8_t r = input; // r will be reversed bits of v; first get LSB of v
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
    void * const port = ws2812_port;
    uint32_t const pin = ws2812_pin;
    uint32_t LedColourData =
        bitReverse(RGB[1]) +        // Green
        (bitReverse(RGB[0]) << 8) + // Red
        (bitReverse(RGB[2]) << 16); // Blue
    uint8_t bits = 24;
    while (bits--) {
        (LedColourData & 0x1) ? ws2812_send_1(port, pin) : ws2812_send_0(port, pin);
        LedColourData >>= 1;
    }
}

void ws2812_init(void)
{
    gpio_port_pin_get(IO_CREATE(WS2812_LED_PIN), &ws2812_port, &ws2812_pin);
    gpio_port_clock((uint32_t)ws2812_port);
    LL_GPIO_SetPinMode(ws2812_port, ws2812_pin, LL_GPIO_MODE_OUTPUT);
    LL_GPIO_SetPinOutputType(ws2812_port, ws2812_pin, LL_GPIO_OUTPUT_PUSHPULL);
    LL_GPIO_SetPinSpeed(ws2812_port, ws2812_pin, LL_GPIO_SPEED_FREQ_HIGH);
    //LL_GPIO_SetPinPull(ws2812_port, ws2812_pin, LL_GPIO_PULL_NO);
    LL_GPIO_ResetOutputPin(ws2812_port, ws2812_pin);
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

#include <stdint.h>
#include "targets.h"

#if (GPIO_PIN_LED_WS2812 != UNDEF_PIN) && (GPIO_PIN_LED_WS2812_FAST != UNDEF_PIN)
#define WS2812_LED_IS_USED 1

#ifndef BRIGHTNESS
#define BRIGHTNESS 10 // 1...256
#endif

static uint32_t current_rgb;

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
#if defined(STM32F1)
        __NOP(); __NOP(); __NOP(); __NOP(); __NOP();
        __NOP(); __NOP(); __NOP(); __NOP(); __NOP();
        __NOP(); __NOP(); __NOP(); __NOP(); __NOP();
        __NOP(); __NOP(); __NOP(); __NOP(); __NOP();
        __NOP(); __NOP(); __NOP(); __NOP(); __NOP();
        __NOP(); __NOP(); __NOP(); __NOP(); __NOP();
        __NOP(); __NOP(); __NOP(); __NOP(); __NOP();
        __NOP(); __NOP(); __NOP(); __NOP(); __NOP();
        __NOP();
#endif
        __NOP(); __NOP(); __NOP(); __NOP();
#if !defined(STM32F1)
        __NOP();
#endif
        digitalWriteFast(GPIO_PIN_LED_WS2812_FAST, LOW);
        __NOP(); __NOP(); __NOP(); __NOP(); __NOP();
        __NOP(); __NOP(); __NOP(); __NOP(); __NOP();
#if defined(STM32F1)
        __NOP(); __NOP(); __NOP(); __NOP(); __NOP();
        __NOP(); __NOP(); __NOP(); __NOP(); __NOP();
        __NOP(); __NOP(); __NOP(); __NOP(); __NOP();
        __NOP(); __NOP(); __NOP(); __NOP(); __NOP();
        __NOP(); __NOP(); __NOP();
#endif
        __NOP(); __NOP(); __NOP(); __NOP();
#if !defined(STM32F1)
        __NOP(); __NOP(); __NOP(); __NOP();
#endif
}

static inline void LEDsend_0(void) {
        digitalWriteFast(GPIO_PIN_LED_WS2812_FAST, HIGH);
        __NOP(); __NOP(); __NOP(); __NOP(); __NOP();
        __NOP(); __NOP(); __NOP(); __NOP(); __NOP();
#if defined(STM32F1)
        __NOP(); __NOP(); __NOP(); __NOP(); __NOP();
        __NOP(); __NOP(); __NOP(); __NOP(); __NOP();
        __NOP(); __NOP(); __NOP(); __NOP(); __NOP();
        __NOP(); __NOP(); __NOP(); __NOP(); __NOP();
        __NOP(); __NOP(); __NOP(); __NOP(); __NOP();
#endif
        __NOP(); __NOP(); __NOP(); __NOP();
#if !defined(STM32F1)
        __NOP(); __NOP(); __NOP(); __NOP();
#endif
        digitalWriteFast(GPIO_PIN_LED_WS2812_FAST, LOW);
        __NOP(); __NOP(); __NOP(); __NOP(); __NOP();
        __NOP(); __NOP(); __NOP(); __NOP(); __NOP();
        __NOP(); __NOP(); __NOP(); __NOP(); __NOP();
        __NOP(); __NOP(); __NOP(); __NOP(); __NOP();
        __NOP(); __NOP(); __NOP(); __NOP(); __NOP();
        __NOP(); __NOP(); __NOP(); __NOP(); __NOP();
        __NOP(); __NOP(); __NOP(); __NOP(); __NOP();
        __NOP(); __NOP(); __NOP(); __NOP(); __NOP();
#if defined(STM32F1)
        __NOP(); __NOP(); __NOP(); __NOP(); __NOP();
        __NOP(); __NOP(); __NOP(); __NOP(); __NOP();
        __NOP(); __NOP(); __NOP(); __NOP(); __NOP();
        __NOP(); __NOP(); __NOP(); __NOP(); __NOP();
        __NOP(); __NOP(); __NOP(); __NOP(); __NOP();
        __NOP(); __NOP(); __NOP(); __NOP(); __NOP();
        __NOP(); __NOP(); __NOP(); __NOP(); __NOP();
        __NOP(); __NOP(); __NOP(); __NOP(); __NOP();
        __NOP(); __NOP(); __NOP(); __NOP();
#endif
        __NOP(); __NOP(); __NOP(); __NOP();
#if !defined(STM32F1)
        __NOP();
#endif
}

void WS281Binit(void) // takes RGB data
{
    pinMode(GPIO_PIN_LED_WS2812, OUTPUT);
}

void WS281BsetLED(uint32_t const RGB) // takes RGB data
{
    /* Check if update is needed */
    if (current_rgb == RGB)
        return;
    current_rgb = RGB;

    // Convert to GRB
    uint32_t GRB = (RGB & 0x0000FF00) << 8;
    GRB |= (RGB & 0x00FF0000) >> 8;
    GRB |= (RGB & 0x000000FF);

    uint32_t bit = 1<<23;
    while (bit)
    {
        noInterrupts();
        (GRB & bit) ? LEDsend_1() : LEDsend_0();
        interrupts();
        bit >>= 1;
    }
    delayMicroseconds(50); // needed to latch in the values
}

#endif /* (GPIO_PIN_LED_WS2812 != UNDEF_PIN) && (GPIO_PIN_LED_WS2812_FAST != UNDEF_PIN) */

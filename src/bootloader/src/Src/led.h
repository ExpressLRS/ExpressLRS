#ifndef __LED_H_
#define __LED_H_

#include <stdint.h>

#if defined(WS2812_LED_PIN)
void ws2812_init(void);
void ws2812_set_color(uint8_t const r, uint8_t const g, uint8_t const b);
void ws2812_set_color_u32(uint32_t const rgb);
#else
#define ws2812_init()
#define ws2812_set_color(...)
#define ws2812_set_color_u32(...)
#endif
#endif /* __LED_H_ */

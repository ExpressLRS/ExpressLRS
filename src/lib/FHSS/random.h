#pragma once

#include <stdint.h>

// the max value returned by rng
#define RNG_MAX 0x7FFF

uint16_t rng(void);

void rngSeed(uint32_t newSeed);
// 0..255 returned
uint8_t rng8Bit(void);
// 0..31 returned
uint8_t rng5Bit(void);

// returns 0 <= x < upper where n < 256
uint8_t rngN(uint8_t upper);

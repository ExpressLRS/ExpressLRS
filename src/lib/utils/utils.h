#pragma once

#include <stdint.h>
#include "platform.h"

// the max value returned by rng
#define RNG_MAX 0x7FFF

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define LIMIT(mi, x, ma) MIN(MAX(mi, x), ma)

extern uint32_t seed;

void rngSeed(uint32_t newSeed);
uint32_t rng(void);
// 0..255 returned
uint32_t rng8Bit(void);
// 0..31 returned
uint32_t rng5Bit(void);
// 0..3 returned
uint32_t rng0to2(void);
// returns 0 <= x < max where max <= 256
uint32_t rngN(uint32_t upper);

unsigned int ICACHE_RAM_ATTR
volatile_memcpy(volatile void *d, volatile void *s, unsigned int n);

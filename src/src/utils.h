#pragma once

// void ICACHE_RAM_ATTR PrintRC()
// }

// the max value returned by rng
#define RNG_MAX 0x7FFF

extern unsigned long seed;

long rng(void);

void rngSeed(long newSeed);
// 0..255 returned
long rng8Bit(void);
// 0..31 returned
long rng5Bit(void);

// returns 0<x<n where n <= 256
unsigned int rngN(unsigned int upper);

unsigned int volatile_memcpy(volatile void *d, volatile void *s, unsigned int n);

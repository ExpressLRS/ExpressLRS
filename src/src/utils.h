#pragma once

// void  PrintRC()
// }

// the max value returned by rng
#define RNG_MAX 0x7FFF

extern unsigned long seed;

long rng(void);

void rngSeed(long newSeed);
// 0..255 returned
uint8_t rng8Bit(void);
// 0..31 returned
long rng5Bit(void);

// returns 0 <= x < n where n <= 256
unsigned int rngN(unsigned int upper);

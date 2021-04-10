#pragma once

// void  PrintRC()
// }

extern unsigned long seed;

// returns values between 0 and 0x7FFF
long rng(void);

void rngSeed(long newSeed);
// 0..255 returned
long rng8Bit(void);
// 0..31 returned
long rng5Bit(void);

// returns 0 <= x < n
unsigned int rngN(unsigned int upper);

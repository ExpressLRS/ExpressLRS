#pragma once

// void ICACHE_RAM_ATTR PrintRC()
// }

extern unsigned long seed;

long rng(void);

void rngSeed(long newSeed);
// 0..255 returned
long rng8Bit(void);
// 0..31 returned
long rng5Bit(void);
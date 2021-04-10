#include "utils.h"

unsigned long seed = 0;

// returns values between 0 and 0x7FFF
long rng(void)
{
    unsigned long m = 2147483648;
    long a = 214013;
    long c = 2531011;
    seed = (a * seed + c) % m;
    return seed >> 16;
}

void rngSeed(long newSeed)
{
    seed = newSeed;
}

// returns 0 <= x < max
unsigned int rngN(unsigned int max)
{
    return rng() % max;
}

// 0..255 returned
long rng8Bit(void)
{
    return rng() & 0b11111111;
}

// 0..31 returned
long rng5Bit(void)
{
    return rng() & 0b11111;
}

// 0..255 returned
long rng0to2(void)
{
    int randomNumber = rng() & 0b11;

    while(randomNumber == 3) {
        randomNumber = rng() & 0b11;
    }
    return randomNumber;
} 
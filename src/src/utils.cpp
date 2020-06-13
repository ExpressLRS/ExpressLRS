#include "utils.h"

unsigned long seed = 0;

// returns values between 0 and 0x7FFF
// NB rngN depends on this output range, so if we change the
// behaviour rngN will need updating
long rng(void)
{
    long m = 2147483648;
    long a = 214013;
    long c = 2531011;
    seed = (a * seed + c) % m;
    return seed >> 16;
}

void rngSeed(long newSeed)
{
    seed = newSeed;
}

// returns 0 <= x < max where max <= 256
// (actual upper limit is higher, but there is one and I haven't
//  thought carefully about what it is)
unsigned int rngN(unsigned int max)
{
    unsigned long x = rng();
    unsigned int result = (x * max) / RNG_MAX;
    return result;
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
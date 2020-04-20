#include "utils.h"
//#include "debug.h"

uint32_t seed = 0;

// returns values between 0 and 0x7FFF
// NB rngN depends on this output range, so if we change the
// behaviour rngN will need updating
uint32_t rng(void)
{
    long m = 2147483648;
    long a = 214013;
    long c = 2531011;
    seed = (a * seed + c) % m;
    return seed >> 16;
}

void rngSeed(uint32_t newSeed)
{
    seed = newSeed;
}

// returns 0<x<max where max <= 256
// (actual upper limit is higher, but there is one and I haven't
//  thought carefully about what it is)
uint32_t rngN(uint32_t max)
{
    uint32_t x = rng();
    uint32_t result = (x * max) / RNG_MAX;
    return result;
}

// 0..255 returned
uint32_t rng8Bit(void)
{
    return rng() & 0b11111111;
}

// 0..31 returned
uint32_t rng5Bit(void)
{
    return rng() & 0b11111;
}

// 0..255 returned
uint32_t rng0to2(void)
{
    uint32_t randomNumber = rng() & 0b11;

    while (randomNumber == 3)
    {
        randomNumber = rng() & 0b11;
    }
    return randomNumber;
}

unsigned int
volatile_memcpy(volatile void *d, volatile void *s, unsigned int n)
{
    volatile unsigned char *dst = (unsigned char *)d;
    volatile unsigned char *src = (unsigned char *)s;
    unsigned int iter;
    for (iter = 0; iter < n; iter++)
    {
        *dst++ = *src++;
    }
    return iter;
}

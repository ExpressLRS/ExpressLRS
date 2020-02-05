#include "debug.h"

// void ICACHE_RAM_ATTR PrintRC()
// {
//   DEBUG_PRINT(crsf.ChannelDataIn[0]);
//   DEBUG_PRINT(",");
//   DEBUG_PRINT(crsf.ChannelDataIn[1]);
//   DEBUG_PRINT(",");
//   DEBUG_PRINT(crsf.ChannelDataIn[2]);
//   DEBUG_PRINT(",");
//   DEBUG_PRINT(crsf.ChannelDataIn[3]);
//   DEBUG_PRINT(",");
//   DEBUG_PRINTLN(crsf.ChannelDataIn[4]);
//   DEBUG_PRINT(",");
//   DEBUG_PRINT(crsf.ChannelDataIn[5]);
//   DEBUG_PRINT(",");
//   DEBUG_PRINTLN(crsf.ChannelDataIn[6]);
//   DEBUG_PRINT(",");
//   DEBUG_PRINTLN(crsf.ChannelDataIn[7]);
// }

unsigned long seed = 0;

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

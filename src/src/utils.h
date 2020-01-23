// void ICACHE_RAM_ATTR PrintRC()
// {
//   Serial.print(crsf.ChannelDataIn[0]);
//   Serial.print(",");
//   Serial.print(crsf.ChannelDataIn[1]);
//   Serial.print(",");
//   Serial.print(crsf.ChannelDataIn[2]);
//   Serial.print(",");
//   Serial.print(crsf.ChannelDataIn[3]);
//   Serial.print(",");
//   Serial.println(crsf.ChannelDataIn[4]);
//   Serial.print(",");
//   Serial.print(crsf.ChannelDataIn[5]);
//   Serial.print(",");
//   Serial.println(crsf.ChannelDataIn[6]);
//   Serial.print(",");
//   Serial.println(crsf.ChannelDataIn[7]);
// }

#include "STM32F1xx.h"
#include "stm32f1xx_hal.h"

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

    while (randomNumber == 3)
    {
        randomNumber = rng() & 0b11;
    }
    return randomNumber;
}


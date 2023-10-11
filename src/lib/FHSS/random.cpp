#include "random.h"

uint32_t randomGeneration::seed = 0;

void randomGeneration::setSeed(const uint32_t newSeed)
{
    randomGeneration::seed = newSeed;
}

uint16_t randomGeneration::random()
{
    // Some hardcored values for randomization
    const uint32_t m = 2147483648;
    const uint32_t a = 214013;
    const uint32_t c = 2531011;
    randomGeneration::seed = (a * randomGeneration::seed + c) % m;
    return randomGeneration::seed >> 16;
}

uint8_t randomGeneration::randomValueByUpperBound(const uint8_t max)
{
    return randomGeneration::random() % max;
}


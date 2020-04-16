#ifndef HELPERS_H_
#define HELPERS_H_

#include <stdint.h>

#define PACKED __attribute__((packed))

// Macros for big-endian (assume little endian host for now) etc
#define BYTE_SWAP_U16(x) ((uint16_t)__builtin_bswap16(x))
#define BYTE_SWAP_U32(x) ((uint32_t)__builtin_bswap32(x))

/* Function copied from Arduino code */
static inline long MAP(long x, long in_min, long in_max, long out_min, long out_max)
{
    long divisor = (in_max - in_min);
    if (divisor == 0)
    {
        return -1; //AVR returns -1, SAM returns 0
    }
    return (x - in_min) * (out_max - out_min) / divisor + out_min;
}

static inline float MAP_F(float x, float in_min, float in_max, float out_min, float out_max)
{
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

static inline uint16_t MAP_U16(uint16_t x, uint16_t in_min, uint16_t in_max, uint16_t out_min, uint16_t out_max)
{
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
};

#endif /* HELPERS_H_ */

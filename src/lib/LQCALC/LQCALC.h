#pragma once

#include <stdint.h>
#include "targets.h"

template <uint8_t N>
class LQCALC
{
public:
    LQCALC(void)
    {
        reset();
    }

    /* Set the bit for the current period to true and update the running LQ */
    void ICACHE_RAM_ATTR add()
    {
        if (currentIsSet())
            return;
        LQArray[LQbyte] |= LQmask;
        LQ += 1;
    }

    /* Start a new period */
    void ICACHE_RAM_ATTR inc()
    {
        // Increment the counter by shifting one bit higher
        // If we've shifted out all the bits, move to next idx
        LQmask = LQmask << 1;
        if (LQmask == 0)
        {
            LQmask = (1 << 0);
            LQbyte += 1;
        }

        // At idx N / 32 and bit N % 32, wrap back to idx=0, bit=0
        if ((LQbyte == (N / 32)) && (LQmask & (1 << (N % 32))))
        {
            LQbyte = 0;
            LQmask = (1 << 0);
        }

        if ((LQArray[LQbyte] & LQmask) != 0)
        {
            LQArray[LQbyte] &= ~LQmask;
            LQ -= 1;
        }
    }

    /* Return the current running total of bits set, in percent */
    uint8_t ICACHE_RAM_ATTR getLQ() const
    {
        // Allow the compiler to optimize out some or all of the
        // math if evenly divisible
        if (100 % N == 0)
            return (uint32_t)LQ * (100 / N);
        else
            return (uint32_t)LQ * 100 / N;
    }

    /* Initialize and zero the history */
    void reset()
    {
        LQ = 0;
        LQbyte = 0;
        LQmask = (1 << 0);
        for (uint8_t i = 0; i < (sizeof(LQArray)/sizeof(LQArray[0])); i++)
            LQArray[i] = 0;
    }

    /*  Return true if the current period was add()ed */
    bool ICACHE_RAM_ATTR currentIsSet() const
    {
        return LQArray[LQbyte] & LQmask;
    }

    /*  Return true if the previous period was add()ed */
    bool ICACHE_RAM_ATTR previousIsSet() const
    {
        // If LQmask is 1, we need the highest bit from the previous array item
        if (LQmask == (1 << 0))
            return LQArray[(LQbyte - 1) % (N / 32)] & (1 << 31);
        else
            return LQArray[LQbyte] & (LQmask >> 1);
    }

private:
    uint8_t LQ;
    uint8_t LQbyte;
    uint32_t LQmask;
    uint32_t LQArray[(N + 31)/32];
};

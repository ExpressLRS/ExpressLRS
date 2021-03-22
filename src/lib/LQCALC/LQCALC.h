#pragma once

#include <Arduino.h>
#include "../../src/targets.h"

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
    bool ICACHE_RAM_ATTR current() const
    {
        return (LQArray[LQbyte] & LQmask);
    }

private:
    uint8_t LQ;
    uint8_t LQbyte;
    uint32_t LQmask;
    uint32_t LQArray[(N + 31)/32];
};

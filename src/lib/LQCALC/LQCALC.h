#pragma once

#include <stdint.h>

template <uint8_t N>
class LQCALC
{
public:
    LQCALC(void)
    {
        reset();
    }

    /* Set the bit for the current period to true and update the running LQ */
    void add()
    {
        if (currentIsSet())
            return;
        LQArray[index] |= LQmask;
        LQ += 1;
    }

    /* Start a new period */
    void inc()
    {
        // Increment the counter by shifting one bit higher
        // If we've shifted out all the bits, move to next idx
        LQmask = LQmask << 1;
        if (LQmask == 0)
        {
            LQmask = (1 << 0);
            index += 1;
        }

        // At idx N / 32 and bit N % 32, wrap back to idx=0, bit=0
        if ((index == (N / 32)) && (LQmask & (1 << (N % 32))))
        {
            index = 0;
            LQmask = (1 << 0);
        }

        if ((LQArray[index] & LQmask) != 0)
        {
            LQArray[index] &= ~LQmask;
            LQ -= 1;
        }

        if (count < N)
          ++count;
    }

    /* Return the current running total of bits set, in percent */
    uint8_t getLQ() const
    {
        return (uint32_t)LQ * 100U / count;
    }

    /* Return the current running total of bits set, up to N */
    uint8_t getLQRaw() const
    {
        return LQ;
    }

    /* Return the number of periods recorded so far, up to N */
    uint8_t getCount() const
    {
        return count;
    }

    /* Return N, the size of the LQ history */
    uint8_t getSize() const
    {
        return N;
    }

    /* Initialize and zero the history */
    void reset()
    {
        LQ = 0;
        index = 0;
        // count is reset to 1 to prevent divide by zero and
        // typical usage pattern is to call add() on the first packet
        // after a reset, which would increase `LQ` over `count`
        count = 1;
        LQmask = (1 << 0);
        for (uint8_t i = 0; i < (sizeof(LQArray)/sizeof(LQArray[0])); i++)
            LQArray[i] = 0;
    }

    /*  Return true if the current period was add()ed */
    bool currentIsSet() const
    {
        return LQArray[index] & LQmask;
    }

private:
    uint8_t LQ;
    uint8_t index; // current position in LQArray
    uint8_t count;
    uint32_t LQmask;
    uint32_t LQArray[(N + 31)/32];
};

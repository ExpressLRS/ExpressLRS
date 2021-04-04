#pragma once
#include <Arduino.h>
#include <stdio.h>
#include "../../src/targets.h"

class PFD
{
private:
    uint32_t timeSamples_nco = 0;
    uint32_t timeSamples_ref = 0;
    int32_t result;
    bool got_ref;
    bool got_nco;

public:

    inline void ref_rising(uint32_t time)
    {
        timeSamples_ref = time;
        got_ref = true;
    }

    inline void nco_rising(uint32_t time)
    {
        timeSamples_nco = time;
        got_nco = true;
    }

    inline void reset()
    {
        got_ref = false;
        got_nco = false;
    }

    inline void calc_result()
    {
        result = (got_ref && got_nco) ? (int32_t)(timeSamples_ref - timeSamples_nco) : 0;
    }

    inline int32_t get_result()
    {
        return result;
    }
};

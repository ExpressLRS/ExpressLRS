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
    void nco_rising(uint32_t time);
    void ref_rising(uint32_t time);

    void reset();

    void calc_result();
    int32_t get_result();
};

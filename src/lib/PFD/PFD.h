#pragma once
#include <Arduino.h>
#include <stdio.h>
#include "../../src/targets.h"

class PFD
{
private:
    /* data */
public:
    //PFD(/* args */);
    //~PFD();

    void nco_rising();
    void ref_rising();

    void update();
    void reset();
    void set_up_down_states();

    int32_t get_result();

    uint8_t PFD_state = 1; // 1 is the default (middle state)

    bool ref_state = 0;
    bool nco_state = 0;

    bool up_state = 0;
    bool down_state = 0;

    uint32_t timeSamples[2] = {0};
    uint8_t timeSamplesCounter = 0;
};

// uint32_t PFD_UP_HighTime;
// uint32_t PFD_DOWN_HighTime;

// uint32_t PFD_UP_HighTime_LPF;
// uint32_t PFD_DOWN_HighTime_LPF;

// uint32_t PFD_UP_BeginHigh;
// uint32_t PFD_DOWN_BeginHigh;

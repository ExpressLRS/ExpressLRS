#pragma once 

#include <Arduino.h>

// interpolation/extrapolation table based on newton backwards difference method. Code simplified and assumes that x timesteps are constant and equal to 1
// based on https://www.geeksforgeeks.org/newton-forward-backward-interpolation/

class NewtonBackInterp
{
private:
    bool needRecalc;
    //uint32_t calcTime; //calculation time for benchmarking

#define polyOrder 2

    int32_t yvals[polyOrder][polyOrder];
    uint32_t xvals[polyOrder] = {0};

    uint16_t prediction;

public:

    void begin();

    bool handle();

    float calcU(float u, int n);

    int fact(int n);

    void update(uint32_t newVal, uint32_t xval);

    void calcBdiffTable();

    void printBdiffTable();

    void interp(uint32_t xval);

    uint16_t getPrediction();
    
};

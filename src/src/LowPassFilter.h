#pragma once
#include "stdint.h"

/////////// Super Simple Fixed Point Lowpass ////////////////

class LPF
{
public:
    int32_t SmoothDataINT = 0;
    int32_t SmoothDataFP = 0;
    int Beta = 3;     // Length = 16
    int FP_Shift = 3; //Number of fractional bits

    LPF(int Beta_, int FP_Shift_)
    {
        Beta = Beta_;
        FP_Shift = FP_Shift_;
    }

    LPF(int Beta_)
    {
        Beta = Beta_;
    }

    LPF()
    {
        Beta = 3;
        FP_Shift = 3;
    }

    int32_t update(int32_t Indata)
    {
        int RawData;
        RawData = Indata;
        RawData <<= FP_Shift; // Shift to fixed point
        SmoothDataFP = (SmoothDataFP << Beta) - SmoothDataFP;
        SmoothDataFP += RawData;
        SmoothDataFP >>= Beta;
        // Don't do the following shift if you want to do further
        // calculations in fixed-point using SmoothData
        SmoothDataINT = SmoothDataFP >> FP_Shift;
        return SmoothDataINT;
    }

    void init(int32_t Indata)
    {
        /*for (int i = 0; i < 255; i++)
        {
            this->update(Indata);
        }*/
        SmoothDataINT = Indata;
        SmoothDataFP = Indata << FP_Shift;
    }
};

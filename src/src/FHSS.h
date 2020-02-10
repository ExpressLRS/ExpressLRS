#pragma once

#include <Arduino.h>
#include "debug.h"

extern uint8_t NumOfFHSSfrequencies;

extern int32_t FreqCorrection;
#define FreqCorrectionMax 50000
#define FreqCorrectionMin -50000

void ICACHE_RAM_ATTR FHSSsetCurrIndex(uint8_t value);

uint8_t ICACHE_RAM_ATTR FHSSgetCurrIndex();

const uint32_t FHSSfreqs433[5] = {
    433420000,
    433920000,
    434420000};

const uint32_t FHSSfreqs915[20] = {
    915500000,
    916100000,
    916700000,
    917300000,

    917900000,
    918500000,
    919100000,
    919700000,

    920300000,
    920900000,
    921500000,
    922100000,

    922700000,
    923300000,
    923900000,
    924500000,

    925100000,
    925700000,
    926300000,
    926900000};

uint8_t FHSSsequence[256] = {0};

uint32_t ICACHE_RAM_ATTR GetInitialFreq()
{
#ifdef Regulatory_Domain_AU_915

    return FHSSfreqs915[FHSSsequence[0]];

#elif defined Regulatory_Domain_AU_433

    return FHSSfreqs433[FHSSsequence[0]];

#endif
}

uint32_t ICACHE_RAM_ATTR FHSSgetCurrFreq()
{
#ifdef Regulatory_Domain_AU_915
 
    return FHSSfreqs915[FHSSsequence[FHSSptr]];

#elif defined Regulatory_Domain_AU_433

    return FHSSfreqs433[FHSSsequence[FHSSptr]];

#endif

    return 0;
}

uint32_t ICACHE_RAM_ATTR FHSSgetNextFreq()
{
    FHSSptr++;

    return FHSSgetCurrFreq();
}

void ICACHE_RAM_ATTR FHSSrandomiseFHSSsequence()
{
    
    DEBUG_PRINT("FHSSsequence[] = ");

    long macSeed = ((long)TxBaseMac[2] << 24) + ((long)TxBaseMac[3] << 16) + ((long)TxBaseMac[4] << 8) + TxBaseMac[5];
    rngSeed(macSeed);

#ifdef Regulatory_Domain_AU_915

    int hopSeqLength = 256;
    int numOfFreqs = 20; 
    int limit = floor(hopSeqLength/numOfFreqs);

    int prev_val = 0;
    int rand = 0;

    int last_InitialFreq = 0;
    int last_InitialFreq_interval = numOfFreqs;

    int tracker[20] = {0};

    for(int i = 0; i < hopSeqLength; i++)
    {

        if(i >= last_InitialFreq + last_InitialFreq_interval)
        {
            rand = FHSSsequence[0];
            last_InitialFreq = i;
        }
        else
        {
            while(rand > numOfFreqs-1 || prev_val == rand || tracker[rand] > limit)
            {
                rand = rng5Bit();
            }

            if(rand == FHSSsequence[0])
            {
                last_InitialFreq = i;
            }
        }
        
        FHSSsequence[i] = rand;
        tracker[rand] = tracker[rand] + 1;
        prev_val = rand;

        DEBUG_PRINT(FHSSsequence[i]);
        DEBUG_PRINT(", ");
    }

#elif defined Regulatory_Domain_AU_433

    int prev_val = rng0to2(); // Randomised so that FHSSsequence[0] can also be 0.
    int rand = 0;

    for(int i = 0; i < 256; i++)
    {
        while(prev_val == rand)
        {
            rand = rng0to2();
        }

        prev_val = rand;
        FHSSsequence[i] = rand;

        DEBUG_PRINT(FHSSsequence[i]);
        DEBUG_PRINT(", ");
    }

#endif
    
    DEBUG_PRINTLN("");
} 

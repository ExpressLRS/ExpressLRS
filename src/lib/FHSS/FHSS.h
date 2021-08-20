#pragma once

#if defined(Regulatory_Domain_AU_915) || defined(Regulatory_Domain_EU_868) || defined(Regulatory_Domain_IN_866) || defined(Regulatory_Domain_FCC_915) || defined(Regulatory_Domain_AU_433) || defined(Regulatory_Domain_EU_433)
#include "SX127xDriver.h"
#elif Regulatory_Domain_ISM_2400
#include "SX1280Driver.h"
#endif

#include "random.h"

#ifdef Regulatory_Domain_AU_915
#define Regulatory_Domain_Index 1
#elif defined Regulatory_Domain_FCC_915
#define Regulatory_Domain_Index 2
#elif defined Regulatory_Domain_EU_868
#define Regulatory_Domain_Index 3
#elif defined Regulatory_Domain_AU_433
#define Regulatory_Domain_Index 4
#elif defined Regulatory_Domain_EU_433
#define Regulatory_Domain_Index 5
#elif defined Regulatory_Domain_ISM_2400
#define Regulatory_Domain_Index 6
#elif defined Regulatory_Domain_IN_866
#define Regulatory_Domain_Index 7
#else
#define Regulatory_Domain_Index 8
#endif

#define FreqCorrectionMax ((int32_t)(100000/FREQ_STEP))
#define FreqCorrectionMin ((int32_t)(-100000/FREQ_STEP))

#define FREQ_HZ_TO_REG_VAL(freq) ((uint32_t)((double)freq/(double)FREQ_STEP))

#define NR_SEQUENCE_ENTRIES 256

extern volatile uint8_t FHSSptr;
extern int32_t FreqCorrection;
extern uint8_t FHSSsequence[NR_SEQUENCE_ENTRIES];
extern const uint32_t FHSSfreqs[];
extern uint_fast8_t sync_channel;

void FHSSrandomiseFHSSsequence(uint32_t seed);
uint32_t FHSSNumEntriess(void);

static inline void FHSSsetCurrIndex(const uint8_t value)
{ // set the current index of the FHSS pointer
    FHSSptr = value;
}

static inline uint8_t FHSSgetCurrIndex()
{ // get the current index of the FHSS pointer
    return FHSSptr;
}

static inline uint32_t GetInitialFreq()
{
    return FHSSfreqs[sync_channel] - FreqCorrection;
}

static inline uint32_t FHSSgetNextFreq()
{
    return FHSSfreqs[FHSSsequence[FHSSptr++]] - FreqCorrection;
}

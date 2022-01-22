#pragma once

#include "targets.h"

#if defined(Regulatory_Domain_AU_915) || defined(Regulatory_Domain_EU_868) || defined(Regulatory_Domain_IN_866) || defined(Regulatory_Domain_FCC_915) || defined(Regulatory_Domain_AU_433) || defined(Regulatory_Domain_EU_433)
#include "SX127xDriver.h"
#elif Regulatory_Domain_ISM_2400
#include "SX1280Driver.h"
#endif

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

extern volatile uint8_t FHSSptr;
extern int32_t FreqCorrection;
extern uint8_t FHSSsequence[];
extern const uint32_t FHSSfreqs[];
extern uint_fast8_t sync_channel;
extern const uint8_t FHSS_SEQUENCE_CNT;

// create and randomise an FHSS sequence
void FHSSrandomiseFHSSsequence(uint8_t UID[6]);
// The number of frequencies for this regulatory domain
uint32_t FHSSgetChannelCount(void);

// get the initial frequency, which is also the sync channel
static inline uint32_t GetInitialFreq()
{
    return FHSSfreqs[sync_channel] - FreqCorrection;
}

// Get the current sequence pointer
static inline uint8_t FHSSgetCurrIndex()
{
    return FHSSptr;
}

// Set the sequence pointer, used by RX on SYNC
static inline void FHSSsetCurrIndex(const uint8_t value)
{
    FHSSptr = value % FHSS_SEQUENCE_CNT;
}

// Advance the pointer to the next hop and return the frequency of that channel
static inline uint32_t FHSSgetNextFreq()
{
    FHSSptr = (FHSSptr + 1) % FHSS_SEQUENCE_CNT;
    uint32_t freq = FHSSfreqs[FHSSsequence[FHSSptr]] - FreqCorrection;
    return freq;
}

// get the number of entries in the FHSS sequence
static inline uint8_t FHSSgetSequenceCount()
{
    return FHSS_SEQUENCE_CNT;
}

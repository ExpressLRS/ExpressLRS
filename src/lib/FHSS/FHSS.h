#pragma once

#include "targets.h"
#include "random.h"

#if defined(RADIO_SX127X)
#define FreqCorrectionMax ((int32_t)(100000/FREQ_STEP))
#elif defined(RADIO_SX128X)
#define FreqCorrectionMax ((int32_t)(200000/FREQ_STEP))
#endif
#define FreqCorrectionMin (-FreqCorrectionMax)

#define FREQ_HZ_TO_REG_VAL(freq) ((uint32_t)((double)freq/(double)FREQ_STEP))

extern volatile uint8_t FHSSptr;
extern int32_t FreqCorrection;
extern uint8_t FHSSsequence[];
extern const uint32_t FHSSfreqs[];
extern uint_fast8_t sync_channel;
extern const uint8_t FHSS_SEQUENCE_CNT;

// create and randomise an FHSS sequence
void FHSSrandomiseFHSSsequence(uint32_t seed);
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

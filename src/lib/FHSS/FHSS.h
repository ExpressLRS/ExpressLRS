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

#if defined(Regulatory_Domain_ISM_2400)
#define MAX_FREQS 80
#else
#define MAX_FREQS 40
#endif

typedef struct {
    uint8_t     _magic[4];
    char        domain[8];
    uint32_t    freq_count;
    uint32_t    freqs[MAX_FREQS];
} fhss_config_t;

extern volatile uint8_t FHSSptr;
extern int32_t FreqCorrection;
extern uint8_t FHSSsequence[];
extern uint_fast8_t sync_channel;
extern const fhss_config_t FHSSconfig;

// create and randomise an FHSS sequence
void FHSSrandomiseFHSSsequence(uint32_t seed);

// The number of frequencies for this regulatory domain
static inline uint32_t FHSSgetChannelCount(void)
{
    return FHSSconfig.freq_count;
}

// get the number of entries in the FHSS sequence
static inline uint8_t FHSSgetSequenceCount()
{
    return (256 / FHSSconfig.freq_count) * FHSSconfig.freq_count;
}

// get the initial frequency, which is also the sync channel
static inline uint32_t GetInitialFreq()
{
    return FHSSconfig.freqs[sync_channel] - FreqCorrection;
}

// Get the current sequence pointer
static inline uint8_t FHSSgetCurrIndex()
{
    return FHSSptr;
}

// Set the sequence pointer, used by RX on SYNC
static inline void FHSSsetCurrIndex(const uint8_t value)
{
    FHSSptr = value % FHSSgetSequenceCount();
}

// Advance the pointer to the next hop and return the frequency of that channel
static inline uint32_t FHSSgetNextFreq()
{
    FHSSptr = (FHSSptr + 1) % FHSSgetSequenceCount();
    uint32_t freq = FHSSconfig.freqs[FHSSsequence[FHSSptr]] - FreqCorrection;
    return freq;
}

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
#define FREQ_SPREAD_SCALE 256

typedef struct {
    const char  *domain;
    uint32_t    freq_start;
    uint32_t    freq_stop;
    uint32_t    freq_count;
} fhss_config_t;

extern volatile uint8_t FHSSptr;
extern uint32_t freq_spread;
extern int32_t FreqCorrection;
extern int32_t FreqCorrection_2;
extern uint8_t FHSSsequence[];
extern uint_fast8_t sync_channel;
extern const fhss_config_t *FHSSconfig;

// create and randomise an FHSS sequence
void FHSSrandomiseFHSSsequence(uint32_t seed);

// The number of frequencies for this regulatory domain
static inline uint32_t FHSSgetChannelCount(void)
{
    return FHSSconfig->freq_count;
}

// get the number of entries in the FHSS sequence
static inline uint16_t FHSSgetSequenceCount()
{
    return (256 / FHSSconfig->freq_count) * FHSSconfig->freq_count;
}

// get the initial frequency, which is also the sync channel
static inline uint32_t GetInitialFreq()
{
    return FHSSconfig->freq_start + (sync_channel * freq_spread / FREQ_SPREAD_SCALE) - FreqCorrection;
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
    uint32_t freq = FHSSconfig->freq_start + (freq_spread * FHSSsequence[FHSSptr] / FREQ_SPREAD_SCALE) - FreqCorrection;
    return freq;
}

static inline const char *getRegulatoryDomain()
{
    return FHSSconfig->domain;
}

// Get frequency offset by half of the domain frequency range
static inline uint32_t FHSSGeminiFreq(uint8_t FHSSsequenceIdx)
{
    uint32_t numfhss = FHSSgetChannelCount();
    uint8_t offSetIdx = (FHSSsequenceIdx + (numfhss / 2)) % numfhss;  
    uint32_t freq = FHSSconfig->freq_start + (freq_spread * offSetIdx / FREQ_SPREAD_SCALE) - FreqCorrection_2;
    return freq;
}

static inline uint32_t FHSSgetGeminiFreq()
{
    return FHSSGeminiFreq(FHSSsequence[FHSSgetCurrIndex()]);
}

static inline uint32_t FHSSgetInitialGeminiFreq()
{
    return FHSSGeminiFreq(sync_channel);
}

#include "FHSS.h"
#include "logging.h"
#include "options.h"
#include <string.h>

#if defined(RADIO_SX127X) || defined(RADIO_LR1121)

#if defined(RADIO_LR1121)
#include "LR1121Driver.h"
#else
#include "SX127xDriver.h"
#endif

const fhss_config_t domains[] = {
    {"AU915",  FREQ_HZ_TO_REG_VAL(915500000), FREQ_HZ_TO_REG_VAL(926900000), 20, 921000000},
    {"FCC915", FREQ_HZ_TO_REG_VAL(903500000), FREQ_HZ_TO_REG_VAL(926900000), 40, 915000000},
    {"EU868",  FREQ_HZ_TO_REG_VAL(863275000), FREQ_HZ_TO_REG_VAL(869575000), 13, 868000000},
    {"IN866",  FREQ_HZ_TO_REG_VAL(865375000), FREQ_HZ_TO_REG_VAL(866950000), 4, 866000000},
    {"AU433",  FREQ_HZ_TO_REG_VAL(433420000), FREQ_HZ_TO_REG_VAL(434420000), 3, 434000000},
    {"EU433",  FREQ_HZ_TO_REG_VAL(433100000), FREQ_HZ_TO_REG_VAL(434450000), 3, 434000000},
    {"US433",  FREQ_HZ_TO_REG_VAL(433250000), FREQ_HZ_TO_REG_VAL(438000000), 8, 434000000},
    {"US433W",  FREQ_HZ_TO_REG_VAL(423500000), FREQ_HZ_TO_REG_VAL(438000000), 20, 434000000},
};

#if defined(RADIO_LR1121)
const fhss_config_t domainsDualBand[] = {
    {
    #if defined(Regulatory_Domain_EU_CE_2400)
        "CE_LBT",
    #else
        "ISM2G4",
    #endif
    FREQ_HZ_TO_REG_VAL(2400400000), FREQ_HZ_TO_REG_VAL(2479400000), 80, 2440000000}
};
#endif

#elif defined(RADIO_SX128X)
#include "SX1280Driver.h"

const fhss_config_t domains[] = {
    {
    #if defined(Regulatory_Domain_EU_CE_2400)
        "CE_LBT",
    #elif defined(Regulatory_Domain_ISM_2400)
        "ISM2G4",
    #endif
    FREQ_HZ_TO_REG_VAL(2400400000), FREQ_HZ_TO_REG_VAL(2479400000), 80, 2440000000}
};
#endif

// Our table of FHSS frequencies. Define a regulatory domain to select the correct set for your location and radio
const fhss_config_t *FHSSconfig;
const fhss_config_t *FHSSconfigDualBand;

// Actual sequence of hops as indexes into the frequency list
uint8_t FHSSsequence[FHSS_SEQUENCE_LEN];
uint8_t FHSSsequence_DualBand[FHSS_SEQUENCE_LEN];

// Which entry in the sequence we currently are on
uint8_t volatile FHSSptr;

// Channel for sync packets and initial connection establishment
uint_fast8_t sync_channel;
uint_fast8_t sync_channel_DualBand;

// Offset from the predefined frequency determined by AFC on Team900 (register units)
int32_t FreqCorrection;
int32_t FreqCorrection_2;

// Frequency hop separation
uint32_t freq_spread;
uint32_t freq_spread_DualBand;

// Variable for Dual Band radios
bool FHSSusePrimaryFreqBand = true;
bool FHSSuseDualBand = false;

uint16_t primaryBandCount;
uint16_t secondaryBandCount;

bool isDomain868()
{
    return strcmp(FHSSconfig->domain, "EU868") == 0;
}

bool isUsingPrimaryFreqBand()
{
    return FHSSusePrimaryFreqBand;
}

/**
Requirements:
1. 0 every n hops
2. No two repeated channels
3. Equal occurance of each (or as even as possible) of each channel
4. Pseudorandom

Approach:
  Fill the sequence array with the sync channel every FHSS_FREQ_CNT
  Iterate through the array, and for each block, swap each entry in it with
  another random entry, excluding the sync channel.

*/
void FHSSrandomiseFHSSsequenceBuild(const uint32_t seed, uint32_t freqCount, uint_fast8_t syncChannel, uint8_t *inSequence)
{
    // reset the pointer (otherwise the tests fail)
    FHSSptr = 0;
    rngSeed(seed);

    // initialize the sequence array
    for (uint16_t i = 0; i < FHSSgetSequenceCount(); i++)
    {
        if (i % freqCount == 0) {
            inSequence[i] = syncChannel;
        } else if (i % freqCount == syncChannel) {
            inSequence[i] = 0;
        } else {
            inSequence[i] = i % freqCount;
        }
    }

    for (uint16_t i = 0; i < FHSSgetSequenceCount(); i++)
    {
        // if it's not the sync channel
        if (i % freqCount != 0)
        {
            uint8_t offset = (i / freqCount) * freqCount;   // offset to start of current block
            uint8_t rand = rngN(freqCount - 1) + 1;         // random number between 1 and FHSS_FREQ_CNT

            // switch this entry and another random entry in the same block
            uint8_t temp = inSequence[i];
            inSequence[i] = inSequence[offset+rand];
            inSequence[offset+rand] = temp;
        }
    }

    // output FHSS sequence
    // for (uint16_t i=0; i < FHSSgetSequenceCount(); i++)
    // {
    //     DBG("%u ",inSequence[i]);
    //     if (i % 10 == 9)
    //         DBGCR;
    // }
    // DBGCR;
}

// ---- GNSS protection: centers and ±half bandwidths ----
// Default: GPS L1/L2/L5. Add Galileo/BeiDou as needed.
static protected_band_t s_protectedBands[] = {
    { 1575420000u, 1500000u }, // GPS L1 1575.42 MHz, ±1.5 MHz
    { 1227600000u, 1500000u }, // GPS L2 1227.60 MHz, ±1.5 MHz
    { 1176450000u, 1500000u }, // GPS L5 1176.45 MHz, ±1.5 MHz
};
static uint8_t  s_protectedBandCount = sizeof(s_protectedBands)/sizeof(s_protectedBands[0]);
static uint32_t s_protectTolHz = 200000u; // extra margin (±200 kHz)

void FHSS_setProtectedBands(const protected_band_t* bands, uint8_t count, uint32_t tol_hz)
{
    // (Optional) expose a way to override at runtime/build
    s_protectedBandCount = (count <= sizeof(s_protectedBands)/sizeof(s_protectedBands[0])) ? count : s_protectedBandCount;
    for (uint8_t i = 0; i < s_protectedBandCount; ++i) s_protectedBands[i] = bands[i];
    s_protectTolHz = tol_hz;
}

static bool in_window(uint64_t f_hz, uint32_t c_hz, uint32_t half_bw_hz, uint32_t tol_hz)
{
    uint64_t lo = (uint64_t)c_hz - (uint64_t)half_bw_hz - tol_hz;
    uint64_t hi = (uint64_t)c_hz + (uint64_t)half_bw_hz + tol_hz;
    return (f_hz >= lo) && (f_hz <= hi);
}

// Check common 2nd/3rd order intermod products for (fA, fB)
// Consider: |fA ± fB|, |2fA ± fB|, |2fB ± fA|, and optionally 3rd order like |3fA ± 2fB|
bool FHSS_isPairGNSSSafe(uint32_t fA_hz, uint32_t fB_hz)
{
    uint64_t a = fA_hz, b = fB_hz;

    // Build candidate IMD products
    uint64_t cand[] = {
        (a > b) ? (a - b) : (b - a), // |fA - fB|
        a + b,                       // fA + fB
        (2*a > b) ? (2*a - b) : (b - 2*a), // |2fA - fB|
        2*a + b,                     // 2fA + fB
        (2*b > a) ? (2*b - a) : (a - 2*b), // |2fB - fA|
        2*b + a,                     // 2fB + fA
        // Uncomment if you want extra caution with higher orders:
        // (3*a > 2*b) ? (3*a - 2*b) : (2*b - 3*a),
        // 3*a + 2*b,
        // (3*b > 2*a) ? (3*b - 2*a) : (2*a - 3*b),
        // 3*b + 2*a,
    };

    for (uint8_t i = 0; i < sizeof(cand)/sizeof(cand[0]); ++i) {
        for (uint8_t k = 0; k < s_protectedBandCount; ++k) {
            if (in_window(cand[i], s_protectedBands[k].center_hz, s_protectedBands[k].half_bw_hz, s_protectTolHz))
                return false; // NOT safe — hits a protected window
        }
    }
    return true; // safe
}

static void FHSS_buildDualBandPairs_GNSSSafe(uint32_t seed)
{
    // Assumes FHSSsequence (primary) and FHSSsequence_DualBand (secondary) are
    // already filled with shuffled channel indices for each band.
    // We’ll walk the primary sequence in order and pick, for each hop i, a secondary
    // hop j that yields a GNSS-safe pair. We maintain a moving pointer over secondary.

    const uint16_t seqLen = FHSSgetSequenceCount(); // typically 256
    uint16_t j = 0; // secondary moving index

    // Convert index -> frequency helpers
    auto idx_to_freq_primary = [](uint8_t chIdx) -> uint32_t {
        return FHSSconfig->freq_start + ((uint32_t)chIdx * freq_spread / FREQ_SPREAD_SCALE);
    };
    auto idx_to_freq_secondary = [](uint8_t chIdx) -> uint32_t {
        return FHSSconfigDualBand->freq_start + ((uint32_t)chIdx * freq_spread_DualBand / FREQ_SPREAD_SCALE);
    };

    // Bounded reshuffle attempts to avoid pathological dead-ends
    const uint8_t MAX_RESHUFFLES = 8;
    uint8_t reshuffles = 0;

retry_build:
    j = 0;
    for (uint16_t i = 0; i < seqLen; ++i) {
        uint8_t priIdx = FHSSsequence[i];
        uint32_t fA = idx_to_freq_primary(priIdx);

        bool found = false;
        for (uint16_t probe = 0; probe < seqLen; ++probe) {
            uint8_t secIdx = FHSSsequence_DualBand[j];
            uint32_t fB = idx_to_freq_secondary(secIdx);
            if (FHSS_isPairGNSSSafe(fA, fB)) {
                // keep this pairing; advance j so we don't reuse the same secondary hop every time
                j = (j + 1) % seqLen;
                found = true;
                break;
            }
            // try next secondary hop (cyclic)
            j = (j + 1) % seqLen;
        }

        if (!found) {
            // Couldn’t find any partner for this primary hop with the current secondary permutation.
            // Deterministically reshuffle secondary using a derived seed and retry.
            if (++reshuffles <= MAX_RESHUFFLES) {
                uint32_t derivedSeed = seed ^ (0x9E3779B9u * reshuffles);
                FHSSrandomiseFHSSsequenceBuild(derivedSeed, FHSSconfigDualBand->freq_count, sync_channel_DualBand, FHSSsequence_DualBand);
                goto retry_build;
            }
            // Give up gracefully: keep the original order (very unlikely) rather than hang.
            break;
        }
    }
}

void FHSSrandomiseFHSSsequence(const uint32_t seed)
{
    FHSSconfig = &domains[firmwareOptions.domain];
    sync_channel = FHSSconfig->freq_count / 2;
    freq_spread = (FHSSconfig->freq_stop - FHSSconfig->freq_start) * FREQ_SPREAD_SCALE / (FHSSconfig->freq_count - 1);
    primaryBandCount = (FHSS_SEQUENCE_LEN / FHSSconfig->freq_count) * FHSSconfig->freq_count;

    DBGLN("Primary Domain %s, %u channels, sync=%u",
        FHSSconfig->domain, FHSSconfig->freq_count, sync_channel);

    FHSSrandomiseFHSSsequenceBuild(seed, FHSSconfig->freq_count, sync_channel, FHSSsequence);

#if defined(RADIO_LR1121)
    FHSSconfigDualBand = &domainsDualBand[0];
    sync_channel_DualBand = FHSSconfigDualBand->freq_count / 2;
    freq_spread_DualBand = (FHSSconfigDualBand->freq_stop - FHSSconfigDualBand->freq_start) * FREQ_SPREAD_SCALE / (FHSSconfigDualBand->freq_count - 1);
    secondaryBandCount = (FHSS_SEQUENCE_LEN / FHSSconfigDualBand->freq_count) * FHSSconfigDualBand->freq_count;

    DBGLN("Dual Domain %s, %u channels, sync=%u",
        FHSSconfigDualBand->domain, FHSSconfigDualBand->freq_count, sync_channel_DualBand);

    FHSSusePrimaryFreqBand = false;
    FHSSrandomiseFHSSsequenceBuild(seed, FHSSconfigDualBand->freq_count, sync_channel_DualBand, FHSSsequence_DualBand);
    FHSSusePrimaryFreqBand = true;

    // now enforce GNSS-safe pairing across the two bands
    FHSS_buildDualBandPairs_GNSSSafe(seed);

#endif
}

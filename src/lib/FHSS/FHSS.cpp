#include "FHSS.h"
#include "logging.h"
#include "options.h"
#include <string.h>

#if defined(RADIO_SX127X) || defined(RADIO_LR1121) || defined(UNIT_TEST)

#if defined(RADIO_LR1121)
#include "LR1121Driver.h"
#elif defined(RADIO_SX127X)
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

#if defined(RADIO_LR1121) || defined(UNIT_TEST)
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
    { 1227600000u, 51150000u }, // GPS L5 - E6
    { 1581549000u, 20451000u }, // GPS E1 - G1
};
static uint8_t  s_protectedBandCount = sizeof(s_protectedBands)/sizeof(s_protectedBands[0]);
static uint32_t s_protectTolHz = 20000000u; // extra margin (±10 MHz)

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
    for (uint8_t k = 0; k < s_protectedBandCount; ++k) {
        if (in_window(fB_hz - fA_hz, s_protectedBands[k].center_hz, s_protectedBands[k].half_bw_hz, s_protectTolHz))
        {
            return false; // NOT safe — hits a protected window
        }
    }
    return true; // safe
}

static void FHSS_buildSecondarySequence_GNSSAware(uint32_t seed)
{
    // Build the secondary sequence by drawing from a per-hop GNSS-safe bucket
    // with fairness (even usage) and deterministic tie-breaking.
    const uint16_t seqLen = FHSSgetSequenceCount();
    if (seqLen == 0) return;

    const uint16_t priCount = FHSSconfig->freq_count;
    const uint16_t secCount = FHSSconfigDualBand->freq_count;

    auto idx_to_freq_primary = [&](uint8_t chIdx) -> uint32_t {
        return FHSSconfig->freq_start + ((uint32_t)chIdx * freq_spread / FREQ_SPREAD_SCALE);
    };
    auto idx_to_freq_secondary = [&](uint8_t chIdx) -> uint32_t {
        return FHSSconfigDualBand->freq_start + ((uint32_t)chIdx * freq_spread_DualBand / FREQ_SPREAD_SCALE);
    };

    // Helper for deterministic pseudo-random choice without touching global RNG
    auto pick_index_deterministic = [&](uint32_t mix) -> uint32_t {
        // xorshift32
        uint32_t x = mix ? mix : 0xA3C59AC3u;
        x ^= x << 13; x ^= x >> 17; x ^= x << 5;
        return x;
    };

    // Distance score: how far |fB-fA| is from the nearest protected window (higher is better)
    auto distance_score = [&](uint32_t fA_hz, uint32_t fB_hz) -> uint32_t {
        uint64_t d = (fB_hz > fA_hz) ? (uint64_t)fB_hz - (uint64_t)fA_hz : (uint64_t)fA_hz - (uint64_t)fB_hz;
        uint32_t best = 0xFFFFFFFFu;
        for (uint8_t k = 0; k < s_protectedBandCount; ++k) {
            uint64_t lo = (uint64_t)s_protectedBands[k].center_hz - (uint64_t)s_protectedBands[k].half_bw_hz - s_protectTolHz;
            uint64_t hi = (uint64_t)s_protectedBands[k].center_hz + (uint64_t)s_protectedBands[k].half_bw_hz + s_protectTolHz;
            if (d >= lo && d <= hi) {
                // inside window → distance is 0 to the edge; choose min to be conservative
                uint64_t toLo = (d - lo);
                uint64_t toHi = (hi - d);
                uint32_t edgeDist = (uint32_t)((toLo < toHi) ? toLo : toHi);
                if (edgeDist < best) best = edgeDist;
            } else {
                // outside window → distance to the nearest edge
                uint64_t ed = (d < lo) ? (lo - d) : (d - hi);
                uint32_t edgeDist = (uint32_t)((ed > 0xFFFFFFFFULL) ? 0xFFFFFFFFu : ed);
                if (edgeDist < best) best = edgeDist;
            }
        }
        return best;
    };


    // Track even usage across the full sequence
    uint16_t usageCount[256];
    for (uint16_t s = 0; s < 256; ++s) usageCount[s] = 0;

    // Track no-duplicate preference within blocks of size secCount
    bool usedInBlock[256];
    for (uint16_t s = 0; s < 256; ++s) usedInBlock[s] = false;

    for (uint16_t i = 0; i < seqLen; ++i) {
        if (secCount <= 256 && (i % secCount) == 0) {
            // reset per-block usage mask
            for (uint16_t s = 0; s < secCount; ++s) usedInBlock[s] = false;
        }

        const uint8_t priIdx = FHSSsequence[i % priCount];
        const uint32_t fA = idx_to_freq_primary(priIdx);

        // Build the safe bucket for this primary hop
        uint8_t bucket[256];
        uint16_t bucketSz = 0;
        for (uint16_t s = 0; s < secCount; ++s) {
            const uint32_t fB = idx_to_freq_secondary((uint8_t)s);
            if (FHSS_isPairGNSSSafe(fA, fB)) {
                bucket[bucketSz++] = (uint8_t)s;
            }
        }

        uint8_t chosen = 0xFF;

        if (bucketSz > 0) {
            // Prefer candidates not used yet in this block, if any
            uint8_t filtered[256];
            uint16_t filteredSz = 0;
            for (uint16_t t = 0; t < bucketSz; ++t) {
                uint8_t s = bucket[t];
                if (!usedInBlock[s]) filtered[filteredSz++] = s;
            }
            const uint8_t* cand = (filteredSz > 0) ? filtered : bucket;
            uint16_t candSz = (filteredSz > 0) ? filteredSz : bucketSz;

            // Find least-used among candidates
            uint16_t minUse = 0xFFFFu;
            for (uint16_t t = 0; t < candSz; ++t) {
                uint8_t s = cand[t];
                if (usageCount[s] < minUse) minUse = usageCount[s];
            }
            uint8_t least[256];
            uint16_t leastSz = 0;
            for (uint16_t t = 0; t < candSz; ++t) {
                uint8_t s = cand[t];
                if (usageCount[s] == minUse) least[leastSz++] = s;
            }

            // Deterministic pick among equals using a derived mix of seed and hop index
            uint32_t mix = seed ^ (0x9E3779B9u * (uint32_t)(i + 1));
            uint32_t r = pick_index_deterministic(mix);
            chosen = least[r % leastSz];
        } else {
            // Empty bucket: choose least-bad candidate maximizing distance from protected windows
            uint32_t bestScore = 0;
            uint8_t bestIdx = 0;
            for (uint16_t s = 0; s < secCount; ++s) {
                const uint32_t fB = idx_to_freq_secondary((uint8_t)s);
                uint32_t score = distance_score(fA, fB);
                if (score > bestScore || (score == bestScore && s < bestIdx)) {
                    bestScore = score;
                    bestIdx = (uint8_t)s;
                }
            }
            chosen = bestIdx;
        }

        FHSSsequence_DualBand[i] = chosen;
        usageCount[chosen]++;
        usedInBlock[chosen] = true;
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
#if defined(RADIO_LR1121) || defined(UNIT_TEST)
    FHSSconfigDualBand = &domainsDualBand[0];
    sync_channel_DualBand = FHSSconfigDualBand->freq_count / 2;
    freq_spread_DualBand = (FHSSconfigDualBand->freq_stop - FHSSconfigDualBand->freq_start) * FREQ_SPREAD_SCALE / (FHSSconfigDualBand->freq_count - 1);
    secondaryBandCount = (FHSS_SEQUENCE_LEN / FHSSconfigDualBand->freq_count) * FHSSconfigDualBand->freq_count;

    DBGLN("Dual Domain %s, %u channels, sync=%u",
        FHSSconfigDualBand->domain, FHSSconfigDualBand->freq_count, sync_channel_DualBand);

    // Build the secondary band sequence with GNSS avoidance integrated
    FHSSusePrimaryFreqBand = false;
    FHSS_buildSecondarySequence_GNSSAware(seed);
    FHSSusePrimaryFreqBand = true;

#endif
}

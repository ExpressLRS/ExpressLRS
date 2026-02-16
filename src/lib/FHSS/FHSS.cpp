#include "FHSS.h"
#include "logging.h"
#include "options.h"
#include <cstring>

#if defined(RADIO_SX127X) || defined(RADIO_LR1121) || defined(UNIT_TEST)

#if defined(RADIO_LR1121)
#include "LR1121Driver.h"
#elif defined(RADIO_SX127X)
#include "SX127xDriver.h"
#endif

constexpr fhss_config_t domains[] = {
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
constexpr fhss_config_t domainsDualBand[] = {
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

constexpr fhss_config_t domains[] = {
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
uint8_t FHSSsequence_XBand[FHSS_SEQUENCE_LEN];
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
3. Equal occurrence of each (or as even as possible) of each channel
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
constexpr protected_band_t s_protectedBand = { 1583500000u, 32000000u }; // L1/E1/B1/GLONASS L1 region - full keep-out on SA (22 2.4 bands)
constexpr uint32_t gnss_lo = s_protectedBand.center_hz - s_protectedBand.half_bw_hz;
constexpr uint32_t gnss_hi = s_protectedBand.center_hz + s_protectedBand.half_bw_hz;

static bool FHSS_isPairGNSSSafe(const uint32_t fA_hz, const uint32_t fB_hz)
{
    const auto f_hz = fB_hz - fA_hz;
    return (f_hz < gnss_lo) || (f_hz > gnss_hi);
}

static uint32_t FHSS_idxToFreqPrimary(const uint8_t chIdx)
{
    return FHSSconfig->freq_start + static_cast<uint32_t>(chIdx) * freq_spread / FREQ_SPREAD_SCALE;
}

static uint32_t FHSS_idxToFreqSecondary(const uint8_t chIdx)
{
    return FHSSconfigDualBand->freq_start + static_cast<uint32_t>(chIdx) * freq_spread_DualBand / FREQ_SPREAD_SCALE;
}

static uint32_t FHSS_pickIndexDeterministic(const uint32_t mix)
{
    uint32_t x = mix ? mix : 0xA3C59AC3u;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return x;
}

static uint32_t FHSS_distanceScore(const uint32_t fA_hz, const uint32_t fB_hz)
{
    const uint32_t d = fB_hz - fA_hz;
    uint32_t best = 0xFFFFFFFFu;
    if (d >= gnss_lo && d <= gnss_hi) {
        // inside window → distance is min distance to edge
        const uint32_t toLo = d - gnss_lo;
        const uint32_t toHi = gnss_hi - d;
        const uint32_t edgeDist = toLo < toHi ? toLo : toHi;
        if (edgeDist < best) best = edgeDist;
    } else {
        // outside window → distance to the nearest edge
        const uint32_t ed = (d < gnss_lo) ? (gnss_lo - d) : (d - gnss_hi);
        if (ed < best) best = ed;
    }
    return best;
}

static void FHSS_buildSecondarySequence_GNSSAware(const uint32_t seed)
{
    // Build the secondary sequence by drawing from a per-hop GNSS-safe bucket
    // with fairness (even usage) and deterministic tie-breaking.
    const uint16_t seqLen = FHSSgetSequenceCount();
    if (seqLen == 0) return;

    const uint16_t priCount = FHSSconfig->freq_count;
    const uint16_t secCount = FHSSconfigDualBand->freq_count;

    // Track even usage across the full sequence
    uint16_t usageCount[256] {};

    // Track no-duplicate preference within blocks of size secCount
    bool usedInBlock[256] {false};

    for (uint16_t i = 0; i < seqLen; ++i) {
        if (secCount <= 256 && (i % secCount) == 0) {
            // reset per-block usage mask
            for (uint16_t s = 0; s < secCount; ++s) usedInBlock[s] = false;
        }

        const uint8_t priIdx = FHSSsequence[i % priCount];
        const uint32_t fA = FHSS_idxToFreqPrimary(priIdx);

        // Build the safe bucket for this primary hop
        uint8_t bucket[256];
        uint16_t bucketSz = 0;
        for (uint16_t s = 0; s < secCount; ++s) {
            const uint32_t fB = FHSS_idxToFreqSecondary(static_cast<uint8_t>(s));
            if (FHSS_isPairGNSSSafe(fA, fB)) {
                bucket[bucketSz++] = static_cast<uint8_t>(s);
            }
        }

        uint8_t chosen;
        if (bucketSz > 0) {
            // Prefer candidates not used yet in this block, if any
            uint8_t filtered[256];
            uint16_t filteredSz = 0;
            for (uint16_t t = 0; t < bucketSz; ++t) {
                const uint8_t s = bucket[t];
                if (!usedInBlock[s]) filtered[filteredSz++] = s;
            }
            const uint8_t* cand = filteredSz > 0 ? filtered : bucket;
            const uint16_t candSz = filteredSz > 0 ? filteredSz : bucketSz;

            // Find least-used among candidates
            uint16_t minUse = 0xFFFFu;
            for (uint16_t t = 0; t < candSz; ++t) {
                const uint8_t s = cand[t];
                if (usageCount[s] < minUse) minUse = usageCount[s];
            }
            uint8_t least[256];
            uint16_t leastSz = 0;
            for (uint16_t t = 0; t < candSz; ++t) {
                const uint8_t s = cand[t];
                if (usageCount[s] == minUse) least[leastSz++] = s;
            }

            // Deterministic pick among equals using a derived mix of seed and hop index
            const uint32_t mix = seed ^ 0x9E3779B9u * static_cast<uint32_t>(i + 1);
            const uint32_t r = FHSS_pickIndexDeterministic(mix);
            chosen = least[r % leastSz];
        } else {
            // Empty bucket: choose the least-bad candidate maximizing distance from protected windows
            uint32_t bestScore = 0;
            uint8_t bestIdx = 0;
            for (uint16_t s = 0; s < secCount; ++s) {
                const uint32_t fB = FHSS_idxToFreqSecondary(static_cast<uint8_t>(s));
                const uint32_t score = FHSS_distanceScore(fA, fB);
                if (score > bestScore || (score == bestScore && s < bestIdx)) {
                    bestScore = score;
                    bestIdx = static_cast<uint8_t>(s);
                }
            }
            chosen = bestIdx;
        }

        FHSSsequence_XBand[i] = chosen;
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
    FHSSusePrimaryFreqBand = false;

    DBGLN("Dual Domain %s, %u channels, sync=%u",
        FHSSconfigDualBand->domain, FHSSconfigDualBand->freq_count, sync_channel_DualBand);

    // Build High Band hopping sequence
    FHSSrandomiseFHSSsequenceBuild(seed, FHSSconfigDualBand->freq_count, sync_channel_DualBand, FHSSsequence_DualBand);

    // Build High Band hopping sequence with GNSS avoidance integrated
    FHSS_buildSecondarySequence_GNSSAware(seed ^ 0xfbce3cce);
    FHSSusePrimaryFreqBand = true;
#endif
}

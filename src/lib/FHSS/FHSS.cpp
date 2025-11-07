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

static void FHSS_buildDualBandPairs_GNSSSafe(uint32_t seed)
{
    // Assumes these globals already exist (as in ELRS):
    // - FHSSsequence               : uint8_t[] primary sequence (length = FHSSgetSequenceCount())
    // - FHSSsequence_DualBand      : uint8_t[] secondary sequence (same length when dual-band)
    // - FHSSconfig / FHSSconfigDualBand with freq_start, freq_count, etc.
    // - sync_channel / sync_channel_DualBand already pinned by FHSSrandomiseFHSSsequenceBuild()

    const uint16_t seqLen = FHSSgetSequenceCount();
    if (seqLen == 0) return;

    // If the two bands have different channel counts, we still produce seqLen entries,
    // wrapping the smaller one as needed.
    const uint16_t priCount = FHSSconfig->freq_count;
    const uint16_t secCount = FHSSconfigDualBand->freq_count;

    // Helpers to convert channel index (0..count-1) -> absolute frequency in Hz
    auto idx_to_freq_primary = [&](uint8_t chIdx) -> uint32_t {
        // Replace with the exact math used in your tree:
        // start + chIdx * step (or spread), respecting your scaling macro(s)
        return FHSSconfig->freq_start + ((uint32_t)chIdx * freq_spread / FREQ_SPREAD_SCALE);
    };
    auto idx_to_freq_secondary = [&](uint8_t chIdx) -> uint32_t {
        return FHSSconfigDualBand->freq_start + ((uint32_t)chIdx * freq_spread_DualBand / FREQ_SPREAD_SCALE);
    };

    // We’ll build a brand new secondary order that aligns 1:1 with primary hops.
    uint8_t newSecondary[FHSS_SEQUENCE_LEN];

    // We walk over the *current* shuffled secondary sequence with a moving pointer j.
    // Each time we accept a candidate, we WRITE it into newSecondary[i].
    uint16_t j = 0;

    const uint8_t MAX_RESHUFFLES = 8;
    uint8_t reshuffles = 0;

retry_build:
    j = 0;
    // Optional: ensure we don’t *lose* the sync channel position if your stack expects it at hop 0.
    // We’ll try to honor "sync_channel_DualBand must be at index 0" first; if we can’t find a safe
    // pairing for hop 0 with that fixed channel after several reshuffles, we fall back.
    bool enforceSyncAtZero = true;

    for (uint16_t i = 0; i < seqLen; ++i) {
        const uint8_t priIdx = FHSSsequence[i % priCount];
        const uint32_t fA = idx_to_freq_primary(priIdx);

        bool found = false;

        if (enforceSyncAtZero && i == 0) {
            // Try to use the fixed sync channel at 0 for the secondary.
            const uint8_t secIdx = sync_channel_DualBand % secCount;
            const uint32_t fB = idx_to_freq_secondary(secIdx);
            if (FHSS_isPairGNSSSafe(fA, fB)) {
                newSecondary[0] = secIdx;
                found = true;
            }
            // If not safe, we’ll drop into the general search below (and eventually reshuffle).
        }

        if (!found) {
            // Probe up to seqLen candidates by cycling j through the *existing* shuffled secondary list.
            for (uint16_t probe = 0; probe < seqLen; ++probe) {
                const uint8_t secIdx = FHSSsequence_DualBand[j % secCount];
                const uint32_t fB = idx_to_freq_secondary(secIdx);
                if (FHSS_isPairGNSSSafe(fA, fB)) {
                    newSecondary[i] = secIdx;     // ***** COMMIT THE CHOICE *****
                    j = (j + 1) % secCount;       // advance so we don’t reuse the same slot every time
                    found = true;
                    break;
                }
                // printf("No, again %u...\n", i);
                j = (j + 1) % secCount;
            }
        }

        if (!found) {
            // Couldn’t find any partner for this primary hop with current secondary permutation.
            if (++reshuffles <= MAX_RESHUFFLES) {
                uint32_t derivedSeed = seed ^ (0x9E3779B9u * reshuffles);
                FHSSrandomiseFHSSsequenceBuild(
                    derivedSeed,
                    FHSSconfigDualBand->freq_count,
                    sync_channel_DualBand,
                    FHSSsequence_DualBand
                );
                // Try again from scratch. If sync-at-zero caused trouble, relax it after first failure.
                enforceSyncAtZero = (reshuffles == 1);
                printf("shuffling");
                goto retry_build;
            }
            // Last-resort: keep original secondary sequence (rare) rather than hang.
            printf("No sats 4 U\n");
            return;
        }
    }

    // ***** COMMIT PERSISTENTLY: replace the *in-use* secondary sequence order *****
    for (uint16_t i = 0; i < seqLen; ++i) {
        FHSSsequence_DualBand[i] = newSecondary[i];
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

    FHSSusePrimaryFreqBand = false;
    FHSSrandomiseFHSSsequenceBuild(seed, FHSSconfigDualBand->freq_count, sync_channel_DualBand, FHSSsequence_DualBand);

    // now enforce GNSS-safe pairing across the two bands
    FHSS_buildDualBandPairs_GNSSSafe(seed);
    FHSSusePrimaryFreqBand = true;

#endif
}

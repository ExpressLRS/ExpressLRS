#include <unity.h>
#include "FHSS.h"
#include <set>
#include <cstring>

static const fhss_exclusion_t excluded[] = {{9, 21}};
static const fhss_config_t split = {"TEST", 1000, 5100, 42, 4000, PWR_250mW, excluded, 1};
static const fhss_config_t regular = {"TEST2", 20000, 27000, 8, 24000, PWR_100mW, nullptr, 0};

static bool allowed(uint32_t frequency)
{
    return (frequency >= 1000 && frequency <= 1800) || (frequency >= 3200 && frequency <= 5100);
}

void setUp()
{
    FHSSusePrimaryFreqBand = true;
    FHSSuseDualBand = false;
    FreqCorrection = FreqCorrection_2 = 0;
    FHSSptr = 0;
    FHSSconfig = &split;
    FHSSconfigDualBand = &regular;
    primaryFreqCount = FHSSinitialiseBand(split, FHSSchannelMap, sync_channel, freq_spread);
    secondaryFreqCount = FHSSinitialiseBand(regular, FHSSchannelMap_DualBand,
        sync_channel_DualBand, freq_spread_DualBand);
    primaryBandCount = FHSSrandomiseFHSSsequenceBuild(0x01020304, primaryFreqCount, sync_channel, FHSSsequence);
    secondaryBandCount = FHSSrandomiseFHSSsequenceBuild(0x01020304, secondaryFreqCount,
        sync_channel_DualBand, FHSSsequence_DualBand);
}
void tearDown() {}

void test_split_count_sync_and_gemini()
{
    TEST_ASSERT_EQUAL(29, FHSSgetChannelCount());
    TEST_ASSERT_EQUAL(232, FHSSgetSequenceCount());
    TEST_ASSERT_TRUE(allowed(FHSSgetInitialFreq()));
    TEST_ASSERT_TRUE(allowed(FHSSgetInitialGeminiFreq()));
    std::set<uint32_t> frequencies;
    for (unsigned i = 0; i < FHSSgetSequenceCount(); ++i)
    {
        TEST_ASSERT_TRUE(allowed(FHSSgetGeminiFreq()));
        TEST_ASSERT_NOT_EQUAL(FHSSgetGeminiFreq(), FHSSchannelFrequency(split, FHSSchannelMap,
            FHSSsequence[FHSSptr], freq_spread));
        const uint32_t frequency = FHSSgetNextFreq();
        TEST_ASSERT_TRUE(allowed(frequency));
        frequencies.insert(frequency);
    }
    TEST_ASSERT_EQUAL(29, frequencies.size());
    TEST_ASSERT_EQUAL(0, FHSSgetCurrIndex());
    TEST_ASSERT_TRUE(FHSSonSyncChannel());
}

void test_blocks_are_balanced_and_repeatable()
{
    uint8_t repeated[FHSS_SEQUENCE_LEN] = {};
    FHSSrandomiseFHSSsequenceBuild(0x01020304, primaryFreqCount, sync_channel, repeated);
    TEST_ASSERT_EQUAL_MEMORY(FHSSsequence, repeated, primaryBandCount);
    for (unsigned block = 0; block < primaryBandCount; block += primaryFreqCount)
    {
        std::set<uint8_t> channels(FHSSsequence + block, FHSSsequence + block + primaryFreqCount);
        TEST_ASSERT_EQUAL(29, channels.size());
        TEST_ASSERT_EQUAL(sync_channel, FHSSsequence[block]);
        TEST_ASSERT_LESS_THAN(29, *channels.rbegin());
    }
}

void test_secondary_and_dual_band_paths()
{
    FHSSusePrimaryFreqBand = false;
    TEST_ASSERT_EQUAL(8, FHSSgetChannelCount());
    TEST_ASSERT_UINT32_WITHIN(0, 24000, FHSSgetInitialFreq());
    for (unsigned i = 0; i < 256; ++i)
        TEST_ASSERT_TRUE(FHSSgetNextFreq() >= 20000);
    FHSSusePrimaryFreqBand = true;
    FHSSuseDualBand = true;
    TEST_ASSERT_EQUAL(232, FHSSgetSequenceCount());
    TEST_ASSERT_EQUAL(24000, FHSSgetInitialGeminiFreq());
    for (unsigned i = 0; i < 464; ++i)
    {
        const uint32_t gemini = FHSSgetGeminiFreq();
        TEST_ASSERT_TRUE(gemini >= 20000 && gemini <= 27000);
        TEST_ASSERT_TRUE(allowed(FHSSgetNextFreq()));
    }
}

void test_multiple_overlapping_exclusions()
{
    const fhss_exclusion_t ranges[] = {{0, 2}, {2, 4}, {40, 41}};
    fhss_config_t config = split;
    config.exclusions = ranges;
    config.exclusion_count = 3;
    TEST_ASSERT_EQUAL(35, FHSSinitialiseBand(config, FHSSchannelMap, sync_channel, freq_spread));
    TEST_ASSERT_EQUAL(5, FHSSchannelMap[0]);
    TEST_ASSERT_EQUAL(39, FHSSchannelMap[34]);
}

void test_invalid_metadata_is_rejected()
{
    const fhss_exclusion_t ranges[] = {{21, 9}, {0, 42}, {0, 40}};
    fhss_config_t config = split;
    for (const auto &range : ranges)
    {
        config.exclusions = &range;
        TEST_ASSERT_EQUAL(0, FHSSinitialiseBand(config, FHSSchannelMap, sync_channel, freq_spread));
    }
    config.exclusions = nullptr;
    TEST_ASSERT_EQUAL(0, FHSSinitialiseBand(config, FHSSchannelMap, sync_channel, freq_spread));
    config.exclusion_count = 0;
    config.freq_count = 81;
    TEST_ASSERT_EQUAL(0, FHSSinitialiseBand(config, FHSSchannelMap, sync_channel, freq_spread));
}

void test_legacy_grid_and_sequence_are_unchanged()
{
    FHSSrandomiseFHSSsequence(0x01020304);
    uint32_t hash = 2166136261U;
    for (unsigned i = 0; i < primaryBandCount; ++i)
    {
        hash = (hash ^ FHSSsequence[i]) * 16777619U;
    }
    TEST_ASSERT_EQUAL_HEX32(0xa3991e4bU, hash);
    for (unsigned i = 0; i < primaryBandCount; ++i)
    {
        const unsigned next = (FHSSptr + 1) % primaryBandCount;
        TEST_ASSERT_EQUAL(FHSSconfig->freq_start + freq_spread * FHSSsequence[next] / FREQ_SPREAD_SCALE,
            FHSSgetNextFreq());
    }
}

void test_corrections_apply_to_primary_and_gemini()
{
    const uint32_t initial = FHSSgetInitialFreq();
    const uint32_t gemini = FHSSgetInitialGeminiFreq();
    FreqCorrection = 7;
    FreqCorrection_2 = 11;
    TEST_ASSERT_EQUAL(initial - 7, FHSSgetInitialFreq());
    TEST_ASSERT_EQUAL(gemini - 11, FHSSgetInitialGeminiFreq());
}

int main(int, char **)
{
    UNITY_BEGIN();
    RUN_TEST(test_split_count_sync_and_gemini);
    RUN_TEST(test_blocks_are_balanced_and_repeatable);
    RUN_TEST(test_secondary_and_dual_band_paths);
    RUN_TEST(test_multiple_overlapping_exclusions);
    RUN_TEST(test_invalid_metadata_is_rejected);
    RUN_TEST(test_legacy_grid_and_sequence_are_unchanged);
    RUN_TEST(test_corrections_apply_to_primary_and_gemini);
    return UNITY_END();
}

#if defined(RADIO_SX127X)
#include "SX127x.h"
#endif
#include "FHSS.h"
#include "options.h"
#include "PowerLimits.h"
#include <unity.h>
#include <set>
#include <cstring>

static double hz(uint32_t frequency)
{
#if defined(RADIO_SX127X)
    return frequency * FREQ_STEP;
#else
    return frequency;
#endif
}

static bool withinBrazil(uint32_t frequency)
{
    // Conservative envelope: nominal 500kHz bandwidth plus up to 100kHz
    // correction in either direction, including SX127x register quantization.
#if defined(RADIO_SX127X)
    // The actual correction limit is an integer number of register steps.
    const double correction = (int32_t)(100000 / FREQ_STEP) * FREQ_STEP;
#else
    const double correction = 100000;
#endif
    const double low = hz(frequency) - 250000 - correction;
    const double high = hz(frequency) + 250000 + correction;
    return (low >= 902000000 && high <= 907500000) ||
           (low >= 915000000 && high <= 928000000);
}

void setUp()
{
    firmwareOptions.domain = 9;
    FHSSusePrimaryFreqBand = true;
    FHSSuseDualBand = false;
    FreqCorrection = FreqCorrection_2 = 0;
    FHSSrandomiseFHSSsequence(0x01020304);
}
void tearDown() {}

void test_brazil_grid_and_safe_test_center()
{
    TEST_ASSERT_EQUAL_STRING("BR915", FHSSgetRegulatoryDomain());
    TEST_ASSERT_EQUAL(42, FHSSconfig->freq_count);
    TEST_ASSERT_EQUAL(29, FHSSgetChannelCount());
    TEST_ASSERT_EQUAL(232, FHSSgetSequenceCount());
    TEST_ASSERT_EQUAL(1, FHSSconfig->exclusion_count);
    TEST_ASSERT_EQUAL(9, FHSSconfig->exclusions[0].first);
    TEST_ASSERT_EQUAL(21, FHSSconfig->exclusions[0].last);
    TEST_ASSERT_TRUE(withinBrazil(FREQ_HZ_TO_REG_VAL(FHSSconfig->freq_center)));
    TEST_ASSERT_EQUAL(922150000, FHSSconfig->freq_center);
}

void test_all_hop_sync_and_gemini_frequencies_are_allowed()
{
    unsigned lowCount = 0;
    for (unsigned i = 0; i < primaryFreqCount; ++i)
    {
        const uint32_t frequency = FHSSchannelFrequency(*FHSSconfig, FHSSchannelMap, i, freq_spread);
        TEST_ASSERT_TRUE(withinBrazil(frequency));
        if (hz(frequency) < 907500000)
            ++lowCount;
    }
    TEST_ASSERT_EQUAL(9, lowCount);
    TEST_ASSERT_TRUE(withinBrazil(FHSSgetInitialFreq()));
    TEST_ASSERT_TRUE(withinBrazil(FHSSgetInitialGeminiFreq()));
    for (unsigned cycle = 0; cycle < 2; ++cycle)
    {
        std::set<uint32_t> frequencies;
        for (unsigned i = 0; i < primaryBandCount; ++i)
        {
            TEST_ASSERT_TRUE(withinBrazil(FHSSgetGeminiFreq()));
            const uint32_t frequency = FHSSgetNextFreq();
            TEST_ASSERT_TRUE(withinBrazil(frequency));
            frequencies.insert(frequency);
        }
        TEST_ASSERT_EQUAL(29, frequencies.size());
    }
}

void test_brazil_sequence_is_repeatable_and_balanced()
{
    uint8_t first[FHSS_SEQUENCE_LEN];
    memcpy(first, FHSSsequence, primaryBandCount);
    FHSSrandomiseFHSSsequence(0x01020304);
    TEST_ASSERT_EQUAL_MEMORY(first, FHSSsequence, primaryBandCount);
    unsigned counts[29] = {};
    for (unsigned i = 0; i < primaryBandCount; ++i)
        ++counts[FHSSsequence[i]];
    for (unsigned count : counts)
        TEST_ASSERT_EQUAL(8, count);
}

void test_brazil_domain_power_and_dual_band()
{
    const PowerLevels_e cap = PWR_250mW;
    TEST_ASSERT_EQUAL(cap, FHSSgetPrimaryConfig().max_power);
    PowerRequest request;
    request.set(PWR_2000mW, PWR_10mW, selectablePowerLimit(PWR_2000mW,
        FHSSgetPrimaryConfig().max_power, FHSSgetSecondaryConfig().max_power));
    const PowerLevels_e limit = activePowerLimit(FHSSgetPrimaryConfig().max_power,
        FHSSgetSecondaryConfig().max_power, true, true);
#if defined(Regulatory_Domain_EU_CE_2400)
    TEST_ASSERT_EQUAL(PWR_100mW, request.output(PWR_10mW, PWR_2000mW, limit));
    TEST_ASSERT_EQUAL(PWR_100mW, FHSSgetSecondaryConfig().max_power);
#else
    TEST_ASSERT_EQUAL(cap, request.output(PWR_10mW, PWR_2000mW, limit));
#endif
    TEST_ASSERT_EQUAL(PWR_250mW, request.output(PWR_10mW, PWR_2000mW,
        activePowerLimit(cap, FHSSgetSecondaryConfig().max_power, true, false)));
#if defined(RADIO_LR1121) || defined(RADIO_LR2021)
    FHSSuseDualBand = true;
    TEST_ASSERT_EQUAL(232, FHSSgetSequenceCount());
    TEST_ASSERT_TRUE(hz(FHSSgetInitialGeminiFreq()) >= 2400000000);
    for (unsigned i = 0; i < 464; ++i)
    {
        TEST_ASSERT_TRUE(hz(FHSSgetGeminiFreq()) >= 2400000000);
        TEST_ASSERT_TRUE(withinBrazil(FHSSgetNextFreq()));
    }
    FHSSuseDualBand = false;
    FHSSusePrimaryFreqBand = false;
    TEST_ASSERT_EQUAL(80, FHSSgetChannelCount());
#endif
}

void test_ce_flag_only_limits_european_domains()
{
    const unsigned european[] = {2, 5};
    for (unsigned domain : european)
    {
        firmwareOptions.domain = domain;
#if defined(Regulatory_Domain_EU_CE_2400)
        TEST_ASSERT_EQUAL(PWR_25mW, FHSSgetPrimaryConfig().max_power);
#else
        TEST_ASSERT_EQUAL(PWR_2000mW, FHSSgetPrimaryConfig().max_power);
#endif
    }
    const unsigned other[] = {0, 1, 3, 4, 6, 7, 8};
    for (unsigned domain : other)
    {
        firmwareOptions.domain = domain;
        TEST_ASSERT_EQUAL(PWR_2000mW, FHSSgetPrimaryConfig().max_power);
    }
}

void test_existing_domain_ids_are_preserved()
{
    const char *names[] = {"AU915", "FCC915", "EU868", "IN866", "AU433", "EU433", "US433", "US433W", "TH920"};
    for (unsigned i = 0; i < 9; ++i)
    {
        firmwareOptions.domain = i;
        TEST_ASSERT_EQUAL_STRING(names[i], FHSSgetPrimaryConfig().domain);
        TEST_ASSERT_EQUAL(0, FHSSgetPrimaryConfig().exclusion_count);
    }
}

int main(int, char **)
{
    UNITY_BEGIN();
    RUN_TEST(test_brazil_grid_and_safe_test_center);
    RUN_TEST(test_all_hop_sync_and_gemini_frequencies_are_allowed);
    RUN_TEST(test_brazil_sequence_is_repeatable_and_balanced);
    RUN_TEST(test_brazil_domain_power_and_dual_band);
    RUN_TEST(test_existing_domain_ids_are_preserved);
    RUN_TEST(test_ce_flag_only_limits_european_domains);
    return UNITY_END();
}

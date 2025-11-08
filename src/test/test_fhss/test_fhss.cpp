#include "options.h"

#include <FHSS.h>
#include <SX1280_Regs.h>
#include <cstdint>
#include <set>
#include <unity.h>

void test_fhss_first(void)
{
    FHSSrandomiseFHSSsequence(0x01020304L);
    TEST_ASSERT_EQUAL(FHSSgetInitialFreq(), FHSSconfig->freq_start + freq_spread * sync_channel / FREQ_SPREAD_SCALE);
}

void test_fhss_assignment(void)
{
    FHSSrandomiseFHSSsequence(0x01020304L);

    const uint32_t numFhss = FHSSgetChannelCount();
    uint32_t initFreq = FHSSgetInitialFreq();

    uint32_t freq = initFreq;
    for (unsigned int i = 0; i < 512; i++) {
        if ((i % numFhss) == 0) {
            TEST_ASSERT_EQUAL(initFreq, freq);
        } else {
            TEST_ASSERT_NOT_EQUAL(initFreq, freq);
        }
        freq = FHSSgetNextFreq();
    }
}

void test_fhss_unique(void)
{
    FHSSrandomiseFHSSsequence(0x01020304L);

    const uint32_t numFhss = FHSSgetChannelCount();
    std::set<uint32_t> freqs;

    for (unsigned int i = 0; i < 256; i++) {
        uint32_t freq = FHSSgetNextFreq();

        if ((i % numFhss) == 0) {
            freqs.clear();
            freqs.insert(freq);
        } else {
            bool inserted = freqs.insert(freq).second;
            TEST_ASSERT_TRUE_MESSAGE(inserted, "Should only see a frequency one time per number initial value");
        }
    }
}

void test_fhss_same(void)
{
    FHSSrandomiseFHSSsequence(0x01020304L);

    const uint32_t numFhss = FHSSgetSequenceCount();

    uint32_t fhss[numFhss];

    for (unsigned int i = 0; i < FHSSgetSequenceCount(); i++) {
        uint32_t freq = FHSSgetNextFreq();
        fhss[i] = freq;
    }

    FHSSrandomiseFHSSsequence(0x01020304L);

    for (unsigned int i = 0; i < FHSSgetSequenceCount(); i++) {
        uint32_t freq = FHSSgetNextFreq();
        TEST_ASSERT_EQUAL(fhss[i],freq);
    }
}

void test_fhss_reg_same_fcc915(void)
{
    firmwareOptions.domain = 1;
    FHSSrandomiseFHSSsequence(0x01020304L);

    uint32_t start = FHSSconfig->freq_start;
    for (unsigned int i = 1; i < FHSSgetSequenceCount(); i++) {
        uint32_t freq = FHSSgetNextFreq();
        uint32_t reg = FREQ_HZ_TO_REG_VAL(start + FHSSsequence[i]*freq_spread);
        TEST_ASSERT_UINT32_WITHIN(1, reg, freq);
    }
}

void test_fhss_reg_same_eu868(void)
{
    firmwareOptions.domain = 2;
    FHSSrandomiseFHSSsequence(0x01020304L);

    uint32_t start = FHSSconfig->freq_start;
    for (unsigned int i = 1; i < FHSSgetSequenceCount(); i++) {
        uint32_t freq = FHSSgetNextFreq();
        uint32_t reg = FREQ_HZ_TO_REG_VAL(start + FHSSsequence[i]*freq_spread);
        TEST_ASSERT_UINT32_WITHIN(1, reg, freq);
    }
}

void test_secondary_uses_all_channels(void)
{
    firmwareOptions.domain = 2;
    // Build sequences (this initializes FHSSconfigDualBand and FHSSsequence_DualBand)
    FHSSrandomiseFHSSsequence(0x0BADB002u);

    // Switch context to secondary band to query its counts deterministically
    bool prevPrimary = FHSSusePrimaryFreqBand;
    const uint32_t priCount = FHSSgetChannelCount();
    FHSSusePrimaryFreqBand = false;
    const uint32_t secCount = FHSSgetChannelCount();
    const uint16_t seqLen = FHSSgetSequenceCount();
    FHSSusePrimaryFreqBand = prevPrimary;

    {
        // Collect all secondary indices used across the built sequence
        std::set<uint32_t> usedIdx;
        for (uint16_t i = 0; i < seqLen; ++i) {
            uint32_t s = FHSSsequence_DualBand[i];
            // Ensure the index is in range
            TEST_ASSERT_TRUE_MESSAGE(s < secCount, "Secondary index out of range");
            usedIdx.insert(s);
        }

        // Expect that every possible secondary channel appears at least once
        TEST_ASSERT_EQUAL_UINT32_MESSAGE(secCount, usedIdx.size(), "Not all secondary channels were used in the sequence");
    }

    {
        // Collect all secondary x-band indices used across the built sequence
        std::set<uint32_t> usedIdx;
        for (uint16_t i = 0; i < seqLen; ++i) {
            uint32_t s = FHSSsequence_XBand[i];
            // Ensure the index is in range
            TEST_ASSERT_TRUE_MESSAGE(s < secCount, "Secondary index out of range");
            usedIdx.insert(s);
        }

        // Expect that every possible secondary channel appears at least once
        TEST_ASSERT_GREATER_THAN_UINT32_MESSAGE(20, usedIdx.size(), "Not enough secondary x-band channels available in the sequence");
    }
}

// Unity setup/teardown
void setUp() {}
void tearDown() {}

int main(int argc, char **argv)
{
    UNITY_BEGIN();
    RUN_TEST(test_fhss_first);
    RUN_TEST(test_fhss_assignment);
    RUN_TEST(test_fhss_unique);
    RUN_TEST(test_fhss_same);
    RUN_TEST(test_fhss_reg_same_fcc915);
    RUN_TEST(test_fhss_reg_same_eu868);
    RUN_TEST(test_secondary_uses_all_channels);
    UNITY_END();

    return 0;
}

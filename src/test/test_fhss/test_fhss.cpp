#include <cstdint>
#include <cstdio>
#include <cstring>
#include <SX1280_Regs.h>
#include <FHSS.h>
#include <unity.h>
#include <set>

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

void test_fhss_reg_same(void)
{
    FHSSrandomiseFHSSsequence(0x01020304L);

    const uint32_t numFhss = FHSSgetSequenceCount();

    uint32_t fhss[numFhss];

    for (unsigned int i = 1; i < FHSSgetSequenceCount(); i++) {
        uint32_t freq = FHSSgetNextFreq();
        uint32_t reg = FREQ_HZ_TO_REG_VAL((2400400000 + FHSSsequence[i]*1000000));
        TEST_ASSERT_UINT32_WITHIN(1, reg, freq);
    }
}

// the channel counts the shipped domains use
static constexpr uint32_t FREQ_COUNTS[] = {3, 4, 8, 13, 20, 40, 80};
// the count a receiver binding on 2.4GHz leaves selected
static constexpr uint16_t OTHER_BAND_COUNT = 240;
static constexpr uint8_t POISON = 0xEE;
static constexpr uint32_t SEED = 0x05060708L;

// A sequence is a function of freqCount alone, so a build must produce the same
// entries whichever band the caller happens to have selected. On a dual band
// receiver that is whichever binding rate was up when the bind landed.
void test_fhss_build_ignores_band_selection(void)
{
    uint8_t reference[FHSS_SEQUENCE_LEN];
    uint8_t sequence[FHSS_SEQUENCE_LEN];

    for (uint32_t freqCount : FREQ_COUNTS)
    {
        char msg[32];
        snprintf(msg, sizeof(msg), "freqCount=%u", (unsigned)freqCount);

        memset(reference, POISON, sizeof(reference));
        memset(sequence, POISON, sizeof(sequence));

        const uint16_t wholeBlocks = FHSSrandomiseFHSSsequenceBuild(SEED, freqCount, freqCount / 2, reference);

        // the state the build used to take its length from
        secondaryBandCount = OTHER_BAND_COUNT;
        FHSSusePrimaryFreqBand = false;

        TEST_ASSERT_EQUAL_UINT16_MESSAGE(wholeBlocks,
            FHSSrandomiseFHSSsequenceBuild(SEED, freqCount, freqCount / 2, sequence), msg);
        TEST_ASSERT_FALSE_MESSAGE(FHSSusePrimaryFreqBand, msg);
        // comparing the whole buffer catches a short build in the poison it
        // leaves behind, and a long one where the reference is still poison
        TEST_ASSERT_EQUAL_MEMORY_MESSAGE(reference, sequence, FHSS_SEQUENCE_LEN, msg);

        // every block holds each channel exactly once
        for (uint16_t block = 0; block < wholeBlocks; block += freqCount)
        {
            std::set<uint8_t> channels(sequence + block, sequence + block + freqCount);
            TEST_ASSERT_EQUAL_UINT32_MESSAGE(freqCount, channels.size(), msg);
            TEST_ASSERT_LESS_THAN_MESSAGE(freqCount, *channels.rbegin(), msg);
        }
    }
}

// Unity setup/teardown
void setUp()
{
    FHSSusePrimaryFreqBand = true;
    FHSSuseDualBand = false;
    primaryBandCount = 0;
    secondaryBandCount = 0;
}
void tearDown() {}

int main(int argc, char **argv)
{
    UNITY_BEGIN();
    RUN_TEST(test_fhss_first);
    RUN_TEST(test_fhss_assignment);
    RUN_TEST(test_fhss_unique);
    RUN_TEST(test_fhss_same);
    RUN_TEST(test_fhss_reg_same);
    RUN_TEST(test_fhss_build_ignores_band_selection);
    UNITY_END();

    return 0;
}

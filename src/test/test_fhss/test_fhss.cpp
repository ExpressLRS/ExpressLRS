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

static uint16_t buildLegacySequence(const uint32_t seed, const uint8_t freqCount,
    const uint8_t syncChannel, uint8_t *sequence)
{
    const uint16_t sequenceCount = (FHSS_SEQUENCE_LEN / freqCount) * freqCount;
    rngSeed(seed);

    for (uint16_t i = 0; i < sequenceCount; i++)
    {
        if (i % freqCount == 0)
        {
            sequence[i] = syncChannel;
        }
        else if (i % freqCount == syncChannel)
        {
            sequence[i] = 0;
        }
        else
        {
            sequence[i] = i % freqCount;
        }
    }

    for (uint16_t i = 0; i < sequenceCount; i++)
    {
        if (i % freqCount != 0)
        {
            const uint8_t offset = (i / freqCount) * freqCount;
            const uint8_t randomIndex = rngN(freqCount - 1) + 1;
            const uint8_t temp = sequence[i];
            sequence[i] = sequence[offset + randomIndex];
            sequence[offset + randomIndex] = temp;
        }
    }

    return sequenceCount;
}

void test_fhss_no_exclusions_preserves_legacy_sequence(void)
{
    static constexpr uint32_t SEEDS[] = {0, 1, SEED, 0xFFFFFFFF};
    uint8_t expected[FHSS_SEQUENCE_LEN];
    uint8_t actual[FHSS_SEQUENCE_LEN];

    for (uint8_t freqCount : FREQ_COUNTS)
    {
        const fhss_config_t config = {"TEST", 0, 0, freqCount, 0, 0, {}};
        const uint8_t syncChannel = freqCount / 2;

        for (uint32_t seed : SEEDS)
        {
            memset(expected, POISON, sizeof(expected));
            memset(actual, POISON, sizeof(actual));

            const uint16_t expectedCount = buildLegacySequence(
                seed, freqCount, syncChannel, expected);
            const uint16_t actualCount = FHSSrandomiseFHSSsequenceBuild(
                seed, &config, syncChannel, actual);

            TEST_ASSERT_EQUAL_UINT16(expectedCount, actualCount);
            TEST_ASSERT_EQUAL_MEMORY(expected, actual, FHSS_SEQUENCE_LEN);
        }
    }
}

// A build uses the config passed to it and must not depend on whichever band
// the caller currently has selected.
void test_fhss_build_ignores_band_selection(void)
{
    uint8_t reference[FHSS_SEQUENCE_LEN];
    uint8_t sequence[FHSS_SEQUENCE_LEN];

    for (uint32_t freqCount : FREQ_COUNTS)
    {
        char msg[32];
        snprintf(msg, sizeof(msg), "freqCount=%u", (unsigned)freqCount);
        const fhss_config_t config = {"TEST", 0, 0, freqCount, 0, 0, {}};

        memset(reference, POISON, sizeof(reference));
        memset(sequence, POISON, sizeof(sequence));

        const uint16_t wholeBlocks = FHSSrandomiseFHSSsequenceBuild(SEED, &config, freqCount / 2, reference);

        // This state used to select an uninitialized config in native tests.
        secondaryBandCount = OTHER_BAND_COUNT;
        FHSSusePrimaryFreqBand = false;

        TEST_ASSERT_EQUAL_UINT16_MESSAGE(wholeBlocks,
            FHSSrandomiseFHSSsequenceBuild(SEED, &config, freqCount / 2, sequence), msg);
        TEST_ASSERT_FALSE_MESSAGE(FHSSusePrimaryFreqBand, msg);
        TEST_ASSERT_EQUAL_MEMORY_MESSAGE(reference, sequence, FHSS_SEQUENCE_LEN, msg);

        // Every complete block holds each channel exactly once.
        for (uint16_t block = 0; block < wholeBlocks; block += freqCount)
        {
            std::set<uint8_t> channels(sequence + block, sequence + block + freqCount);
            TEST_ASSERT_EQUAL_UINT32_MESSAGE(freqCount, channels.size(), msg);
            TEST_ASSERT_LESS_THAN_MESSAGE(freqCount, *channels.rbegin(), msg);
        }
    }
}

static constexpr uint32_t BR915_FREQ_COUNT = 42;
static constexpr uint8_t BR915_EXCLUDED_START = 9;
static constexpr uint8_t BR915_EXCLUDED_END = 21;
static constexpr uint32_t BR915_USABLE_COUNT =
    BR915_FREQ_COUNT - (BR915_EXCLUDED_END - BR915_EXCLUDED_START + 1);
static const fhss_config_t BR915_CONFIG = {
    "BR915", 902400000, 927600000, BR915_FREQ_COUNT, 915000000, 1,
    {{BR915_EXCLUDED_START, BR915_EXCLUDED_END}}
};

void test_fhss_build_excludes_channels_without_overflow(void)
{
    const fhss_config_t &config = BR915_CONFIG;
    uint8_t sequence[FHSS_SEQUENCE_LEN];
    memset(sequence, POISON, sizeof(sequence));

    const uint16_t sequenceCount = FHSSrandomiseFHSSsequenceBuild(SEED, &config, 22, sequence);
    TEST_ASSERT_EQUAL_UINT16((FHSS_SEQUENCE_LEN / BR915_USABLE_COUNT) * BR915_USABLE_COUNT, sequenceCount);

    for (uint16_t block = 0; block < sequenceCount; block += BR915_USABLE_COUNT)
    {
        std::set<uint8_t> channels(sequence + block, sequence + block + BR915_USABLE_COUNT);
        TEST_ASSERT_EQUAL_UINT32(BR915_USABLE_COUNT, channels.size());
        for (uint8_t channel : channels)
        {
            TEST_ASSERT_FALSE(channel >= BR915_EXCLUDED_START && channel <= BR915_EXCLUDED_END);
            TEST_ASSERT_LESS_THAN(BR915_FREQ_COUNT, channel);
        }
    }

    // A partial trailing block must remain untouched.
    for (uint16_t i = sequenceCount; i < FHSS_SEQUENCE_LEN; i++)
    {
        TEST_ASSERT_EQUAL_HEX8(POISON, sequence[i]);
    }
}

void test_br915_gemini_frequencies_exclude_keep_out_zone(void)
{
    FHSSconfig = &BR915_CONFIG;
    freq_spread = (BR915_CONFIG.freq_stop - BR915_CONFIG.freq_start) *
        FREQ_SPREAD_SCALE / (BR915_CONFIG.freq_count - 1);
    sync_channel = 22;
    primaryBandCount = FHSSrandomiseFHSSsequenceBuild(
        SEED, FHSSconfig, sync_channel, FHSSsequence);

    for (uint16_t i = 0; i < primaryBandCount; i++)
    {
        FHSSptr = i;
        const uint8_t pairedChannel = FHSSgetGeminiChannel(FHSSconfig, FHSSsequence[i]);
        TEST_ASSERT_FALSE(
            pairedChannel >= BR915_EXCLUDED_START && pairedChannel <= BR915_EXCLUDED_END);

        const uint32_t expectedFrequency = FHSSconfig->freq_start +
            (freq_spread * pairedChannel / FREQ_SPREAD_SCALE);
        TEST_ASSERT_EQUAL_UINT32(expectedFrequency, FHSSgetGeminiFreq());
    }

    const uint8_t initialPair = FHSSgetGeminiChannel(FHSSconfig, sync_channel);
    TEST_ASSERT_FALSE(
        initialPair >= BR915_EXCLUDED_START && initialPair <= BR915_EXCLUDED_END);
    TEST_ASSERT_EQUAL_UINT32(
        FHSSconfig->freq_start + (freq_spread * initialPair / FREQ_SPREAD_SCALE),
        FHSSgetInitialGeminiFreq());
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
    RUN_TEST(test_fhss_no_exclusions_preserves_legacy_sequence);
    RUN_TEST(test_fhss_build_ignores_band_selection);
    RUN_TEST(test_fhss_build_excludes_channels_without_overflow);
    RUN_TEST(test_br915_gemini_frequencies_exclude_keep_out_zone);
    UNITY_END();

    return 0;
}

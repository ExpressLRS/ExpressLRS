#include <unity.h>

#include "freqTable.h"

// Indexes come from MSP (FC, handset, OTA uplink) and can be 0..255

void test_freq_in_range()
{
    TEST_ASSERT_EQUAL_UINT16(5865, getFreqByIdx(0));                   // A1
    TEST_ASSERT_EQUAL_UINT16(5621, getFreqByIdx(FREQ_TABLE_SIZE - 1)); // L8
    TEST_ASSERT_EQUAL_UINT8('B', channelFreqLabelByIdx(0));
    TEST_ASSERT_EQUAL_UINT8('W', channelFreqLabelByIdx(FREQ_TABLE_SIZE - 1));
    TEST_ASSERT_EQUAL_UINT8('A', getBandLetterByIdx(0));
    TEST_ASSERT_EQUAL_UINT8('L', getBandLetterByIdx(getFreqTableBands() - 1));
}

void test_freq_out_of_range_returns_zero()
{
    for (unsigned idx = FREQ_TABLE_SIZE; idx <= 255; idx++)
    {
        TEST_ASSERT_EQUAL_UINT16(0, getFreqByIdx(idx));
        TEST_ASSERT_EQUAL_UINT8(0, channelFreqLabelByIdx(idx));
    }
}

void test_band_letter_out_of_range_returns_zero()
{
    for (unsigned idx = getFreqTableBands(); idx <= 255; idx++)
    {
        TEST_ASSERT_EQUAL_UINT8(0, getBandLetterByIdx(idx));
    }
}

void setUp() {}
void tearDown() {}

int main(int argc, char **argv)
{
    UNITY_BEGIN();
    RUN_TEST(test_freq_in_range);
    RUN_TEST(test_freq_out_of_range_returns_zero);
    RUN_TEST(test_band_letter_out_of_range_returns_zero);
    UNITY_END();
    return 0;
}

#include <cstdint>
#include <math.h>
#include <FHSS.h>
#include <unity.h>
#include <iostream>
#include <bitset>

#include "helpers.h"

void test_fhss_assignment(void)
{
    FHSSrandomiseFHSSsequence(0x01020304L);

    uint32_t initFreq = GetInitialFreq();

    FHSSptr=255;
    for (unsigned int i = 0; i < 256; i++) {
        uint32_t freq = FHSSgetNextFreq();
        if (i%(NR_FHSS_ENTRIES-1) == 0) {
            TEST_ASSERT_EQUAL(initFreq, freq);
        } else {
            TEST_ASSERT_NOT_EQUAL(initFreq, freq);
        }
    }
}

int main(int argc, char **argv)
{
    UNITY_BEGIN();
    RUN_TEST(test_fhss_assignment);
    UNITY_END();

    return 0;
}

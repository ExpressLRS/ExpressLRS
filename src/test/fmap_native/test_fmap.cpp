#if PLATFORM_PIC32
#include <stdint.h> // This is included in the pic32 compiler downloaded by platformio
#else
#include <cstdint>
#endif
#include <crsf_protocol.h>
#include <unity.h>
#include <iostream>
#include <bitset>

static uint16_t fmapf(uint16_t x, float in_min, float in_max, float out_min, float out_max) { return round((x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min); };

void test_fmap_consistent_with_float(void)
{
    for (int i = 172; i <= 1811; i++)
    {
        uint16_t iv = fmap(i, 172, 1811, 0, 1023);
        uint16_t fv = fmapf(i, 172, 1811, 0, 1023);

        // assert that the integer function is equivalent to the floating point function
        TEST_ASSERT_EQUAL(fv, iv);
    }
}

void test_fmap_consistent_bider(void)
{
    for (int i = 172; i <= 1811; i++)
    {
        uint16_t iv = fmap(i, 172, 1811, 0, 1023);
        uint16_t iiv = fmap(iv, 0, 1023, 172, 1811);
        uint16_t fiv = fmapf(iv, 0, 1023, 172, 1811);

        // make sure that integer and floats return the same values
        TEST_ASSERT_EQUAL(fiv, iiv);

        // make sure that the full cycle value is +/- 1 error given that the input range is under 2x the output range
        TEST_ASSERT_GREATER_OR_EQUAL(i-1, iiv);
        TEST_ASSERT_LESS_OR_EQUAL(i+1, iiv);
    }
}

int main(int argc, char **argv)
{
    UNITY_BEGIN();
    RUN_TEST(test_fmap_consistent_with_float);
    RUN_TEST(test_fmap_consistent_bider);
    UNITY_END();

    return 0;
}

#include <cmath>
#include <unity.h>

// The Baro lib needs Wire so is ignored on native, pull in just the pure math
#include "../../lib/Baro/baro_base.cpp"

/// @brief Standard atmosphere pressure (dPa) at altitude (m), the inverse of the ESP32 formula
static uint32_t pressureAtAltitude(double alt_m)
{
    return lround(1013250.0 * pow(1.0 - alt_m / 44330.0, 1.0 / 0.1903));
}

void test_baro_table_points(void)
{
    // The LUT points themselves should come back exact
    const uint32_t pressures[] = { 1013250, 898750, 794950, 701080, 616400, 540200 };
    for (int i = 0; i < 6; i++)
        TEST_ASSERT_EQUAL_INT32(i * 100000, BaroBase::pressureToAltitude(pressures[i]));
}

void test_baro_accuracy(void)
{
    // Linear interpolation between 1000m points is good to ~12m, anything worse is a bug
    for (int alt_m = 0; alt_m <= 5000; alt_m += 10)
    {
        const int32_t alt_cm = BaroBase::pressureToAltitude(pressureAtAltitude(alt_m));
        TEST_ASSERT_INT32_WITHIN(1500, alt_m * 100, alt_cm);
    }
}

void test_baro_monotonic(void)
{
    // Including extrapolation below sea level and above 5000m, every step of
    // altitude up must read as higher. The old int32 map() overflowed ~180m into
    // each segment and jumped around wildly
    int32_t last_cm = INT32_MIN;
    for (int alt_m = -500; alt_m <= 8000; alt_m += 10)
    {
        const int32_t alt_cm = BaroBase::pressureToAltitude(pressureAtAltitude(alt_m));
        TEST_ASSERT_GREATER_THAN_INT32(last_cm, alt_cm);
        last_cm = alt_cm;
    }
}

// Unity setup/teardown
void setUp() {}
void tearDown() {}

int main(int argc, char **argv)
{
    UNITY_BEGIN();
    RUN_TEST(test_baro_table_points);
    RUN_TEST(test_baro_accuracy);
    RUN_TEST(test_baro_monotonic);
    UNITY_END();

    return 0;
}

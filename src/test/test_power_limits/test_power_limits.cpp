#include <unity.h>
#include "PowerLimits.h"
#include "FHSS.h"

void test_domain_metadata_before_sequence_initialization()
{
#if defined(Regulatory_Domain_EU_CE_2400)
    TEST_ASSERT_EQUAL(PWR_100mW, FHSSgetPrimaryConfig().max_power);
#else
    TEST_ASSERT_EQUAL(PWR_2000mW, FHSSgetPrimaryConfig().max_power);
#endif
    TEST_ASSERT_EQUAL(FHSSgetPrimaryConfig().max_power, FHSSgetSecondaryConfig().max_power);
}

void test_ce_selection_and_output_limits()
{
    TEST_ASSERT_EQUAL(PWR_100mW, selectablePowerLimit(PWR_2000mW, PWR_25mW, PWR_100mW));
    TEST_ASSERT_EQUAL(PWR_50mW, selectablePowerLimit(PWR_50mW, PWR_25mW, PWR_100mW));
    TEST_ASSERT_EQUAL(PWR_25mW, activePowerLimit(PWR_25mW, PWR_100mW, true, false));
    TEST_ASSERT_EQUAL(PWR_100mW, activePowerLimit(PWR_25mW, PWR_100mW, false, false));
    TEST_ASSERT_EQUAL(PWR_25mW, activePowerLimit(PWR_25mW, PWR_100mW, true, true));
    TEST_ASSERT_EQUAL(PWR_25mW, activePowerLimit(PWR_25mW, PWR_100mW, false, true));
}

void test_band_changes_retain_request_and_restrict_output()
{
    // A single request is re-evaluated as the mode changes; the capped output
    // must not replace it, or returning to 2.4GHz would get stuck at 25mW.
    PowerRequest requested;
    TEST_ASSERT_FALSE(requested.initialized());
    requested.set(PWR_100mW, PWR_10mW, PWR_100mW);
    TEST_ASSERT_TRUE(requested.initialized());
    const bool primaryModes[] = {false, true, true, false};
    const bool dualModes[] = {false, false, true, false};
    const PowerLevels_e expected[] = {PWR_100mW, PWR_25mW, PWR_25mW, PWR_100mW};
    for (unsigned i = 0; i < 4; ++i)
    {
        const PowerLevels_e cap = activePowerLimit(PWR_25mW, PWR_100mW, primaryModes[i], dualModes[i]);
        TEST_ASSERT_EQUAL(expected[i], requested.output(PWR_10mW, PWR_2000mW, cap));
        TEST_ASSERT_EQUAL(PWR_100mW, requested.value());
    }
    requested.reset();
    TEST_ASSERT_FALSE(requested.initialized());
}

void test_domain_cap_applies_to_every_power_request()
{
    // Includes high saved settings and increments from dynamic power. This
    // hypothetical cap exercises the infrastructure without adding a domain.
    for (int request = PWR_10mW; request < PWR_COUNT; ++request)
    {
        const PowerLevels_e output = clampOutputPower((PowerLevels_e)request,
            PWR_10mW, PWR_2000mW, PWR_250mW);
        TEST_ASSERT_LESS_OR_EQUAL(PWR_250mW, output);
        if (request <= PWR_250mW)
            TEST_ASSERT_EQUAL(request, output);
    }
}

void test_saved_requests_obey_selection_limits()
{
    PowerRequest request;
    request.set(PWR_2000mW, PWR_10mW, PWR_100mW);
    TEST_ASSERT_EQUAL(PWR_100mW, request.value());
    TEST_ASSERT_EQUAL(PWR_25mW, request.output(PWR_10mW, PWR_2000mW, PWR_25mW));
    request.set(PWR_10mW, PWR_25mW, PWR_100mW);
    TEST_ASSERT_EQUAL(PWR_25mW, request.value());
}

void test_hardware_limits_and_legacy_minimum_fallback()
{
    TEST_ASSERT_EQUAL(PWR_50mW, clampOutputPower(PWR_1000mW, PWR_10mW, PWR_50mW, PWR_250mW));
    TEST_ASSERT_EQUAL(PWR_25mW, clampOutputPower(PWR_10mW, PWR_25mW, PWR_2000mW, PWR_250mW));
    TEST_ASSERT_EQUAL(PWR_50mW, clampOutputPower(PWR_100mW, PWR_50mW, PWR_2000mW, PWR_25mW));
}

void setUp() {}
void tearDown() {}

int main(int, char **)
{
    UNITY_BEGIN();
    RUN_TEST(test_domain_metadata_before_sequence_initialization);
    RUN_TEST(test_ce_selection_and_output_limits);
    RUN_TEST(test_band_changes_retain_request_and_restrict_output);
    RUN_TEST(test_domain_cap_applies_to_every_power_request);
    RUN_TEST(test_saved_requests_obey_selection_limits);
    RUN_TEST(test_hardware_limits_and_legacy_minimum_fallback);
    return UNITY_END();
}

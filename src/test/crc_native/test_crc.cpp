#include <cstdint>
#include <unity.h>
#include "ucrc_t.h"
#include <crc.h>


void test_crc13(void)
{
    uint8_t bytes[] = {45, 46, 47, 48, 49, 50, 51};

    uCRC_t ccrc = uCRC_t("CRC13", 13, 0x3d2f, 0, false, false, 0);
    uint64_t crc = ccrc.get_raw_crc(bytes, 7, 0);

    GENERIC_CRC13 ecrc = GENERIC_CRC13(0x1d2f);
    uint16_t c = ecrc.calc(bytes, 7, 0);

    TEST_ASSERT_EQUAL((int)(crc & 0x1FFF), c);
}

void test_crc8(void)
{
    // Size of a CRSF
    uint8_t bytes[] = {45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55};

    uCRC_t ccrc = uCRC_t("CRC8", 8, 0x107, 0, false, false, 0);
    uint64_t crc = ccrc.get_raw_crc(bytes, 7, 0);

    GENERIC_CRC8 ecrc = GENERIC_CRC8(0x07);
    uint16_t c = ecrc.calc(bytes, 7);

    TEST_ASSERT_EQUAL((int)(crc & 0xFF), c);
}

int main(int argc, char **argv)
{
    UNITY_BEGIN();
    RUN_TEST(test_crc13);
    RUN_TEST(test_crc8);
    UNITY_END();

    return 0;
}

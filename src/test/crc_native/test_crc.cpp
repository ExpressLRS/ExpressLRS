#include <cstdint>
#include <unity.h>
#include "ucrc_t.h"
#include <crc.h>


void test_crc(void)
{
    uint8_t bytes[] = {45, 46, 47, 48, 49, 50, 51};

    uCRC_t ccrc = uCRC_t("CRC13", 13, 0x3d2f, 0, false, false, 0);
    uint64_t crc = ccrc.get_raw_crc(bytes, 7, 0);

    GENERIC_CRC13 ecrc = GENERIC_CRC13(0x1d2f);
    uint16_t c = ecrc.calc(bytes, 7, 0);

    TEST_ASSERT_EQUAL((int)(crc & 0x1FFF), c);
}

int main(int argc, char **argv)
{
    UNITY_BEGIN();
    RUN_TEST(test_crc);
    UNITY_END();

    return 0;
}

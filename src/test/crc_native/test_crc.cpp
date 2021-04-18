#include <cstdint>
#include <unity.h>
#include "ucrc_t.h"
#include <crc.h>

void test_crc13(void)
{
    uint8_t bytes[7];
    for (int i = 0; i < sizeof(bytes); i++)
        bytes[i] = random() % 255;

    uCRC_t ccrc = uCRC_t("CRC13", 13, 0x3d2f, 0, false, false, 0);
    uint64_t crc = ccrc.get_raw_crc(bytes, 7, 0);

    GENERIC_CRC13 ecrc = GENERIC_CRC13(0x1d2f);
    uint16_t c = ecrc.calc(bytes, 7, 0);

    char buf[80];
    char hex[4];
    sprintf(buf, "bytes ");
    for (int i=0 ; i<sizeof(bytes) ; i++) {
        sprintf(hex, "%02x ", bytes[i]);
        strcat(buf, hex);
    }
    TEST_ASSERT_EQUAL_MESSAGE((int)(crc & 0x1FFF), c, buf);
}

void test_crc13_flip6(void)
{
    uint8_t bytes[7];
    for (int i = 0; i < sizeof(bytes); i++)
        bytes[i] = random() % 255;

    GENERIC_CRC13 ccrc = GENERIC_CRC13(0x1d2f);
    uint16_t c = ccrc.calc(bytes, 7, 0);

    // Flip 6 random bits
    for (int i=0 ; i<6 ; i++) {
        int pos = random() % 52;
        bytes[pos/8] ^= 1 << (pos % 8);
    }

    GENERIC_CRC13 ecrc = GENERIC_CRC13(0x1d2f);
    uint16_t e = ecrc.calc(bytes, 7, 0);

    char buf[80];
    char hex[4];
    sprintf(buf, "bytes ");
    for (int i = 0; i < sizeof(bytes); i++)
    {
        sprintf(hex, "%02x ", bytes[i]);
        strcat(buf, hex);
    }
    TEST_ASSERT_NOT_EQUAL_MESSAGE(c, e, buf);
}

void test_crc8(void)
{
    // Size of a CRSF packet
    uint8_t bytes[11];
    for (int i = 0; i < sizeof(bytes); i++)
        bytes[i] = random() % 255;

    uCRC_t ccrc = uCRC_t("CRC8", 8, 0x107, 0, false, false, 0);
    uint64_t crc = ccrc.get_raw_crc(bytes, 7, 0);

    GENERIC_CRC8 ecrc = GENERIC_CRC8(0x07);
    uint16_t c = ecrc.calc(bytes, 7);

    char buf[80];
    char hex[4];
    sprintf(buf, "bytes ");
    for (int i = 0; i < sizeof(bytes); i++)
    {
        sprintf(hex, "%02x ", bytes[i]);
        strcat(buf, hex);
    }
    TEST_ASSERT_EQUAL_MESSAGE((int)(crc & 0xFF), c, buf);
}

int main(int argc, char **argv)
{
    UNITY_BEGIN();
    RUN_TEST(test_crc13);
    RUN_TEST(test_crc13_flip6);
    RUN_TEST(test_crc8);
    UNITY_END();

    return 0;
}

#include <cstdint>
#include <unity.h>
#include "ucrc_t.h"
#include <crc.h>
#include "common.h"

#ifdef BIG_TEST
#define NUM_ITERATIONS 1000000
#else
#define NUM_ITERATIONS 1000
#endif

static char *genMsg(uint8_t bytes[], int len) {
    static char buf[80];
    char hex[4];
    sprintf(buf, "bytes ");
    for (int i = 0; i < len; i++)
    {
        sprintf(hex, "%02x ", bytes[i]);
        strcat(buf, hex);
    }
    return buf;
}

void test_crc13_implementation_compatibility(void)
{
    uint8_t bytes[7];
    for (int i = 0; i < sizeof(bytes); i++)
        bytes[i] = random() % 255;
    bytes[0] &= 0b11;

    uCRC_t ccrc = uCRC_t("CRC13", 13, ELRS_CRC13_POLY, 0, false, false, 0);
    uint64_t crc = ccrc.get_raw_crc(bytes, 7, 0);

    GENERIC_CRC13 ecrc = GENERIC_CRC13(ELRS_CRC13_POLY);
    uint16_t c = ecrc.calc(bytes, 7, 0);

    TEST_ASSERT_EQUAL_MESSAGE((int)(crc & 0x1FFF), c, genMsg(bytes, sizeof(bytes)));
}

// This test will fail as 6 bits is over our HD fro CRC13
void test_crc13_flip6(void)
{
    GENERIC_CRC13 ccrc = GENERIC_CRC13(ELRS_CRC13_POLY);

    for (int x=0 ; x<1000 ; x++) {
        uint8_t bytes[7];
        for (int i = 0; i < sizeof(bytes); i++)
            bytes[i] = random() % 255;
        bytes[0] &= 0b11;

        uint16_t c = ccrc.calc(bytes, 7, 0);

        // Flip 5 random bits
        for (int i=0 ; i<5 ; i++) {
            int pos = random() % 52;
            bytes[pos/8] ^= 1 << (pos % 8);
        }

        uint16_t e = ccrc.calc(bytes, 7, 0);
        TEST_ASSERT_NOT_EQUAL_MESSAGE(c, e, genMsg(bytes, sizeof(bytes)));

        // Flip all the bits one after the other
        for (int i=0 ; i<50 ; i++) {
            // flip bit i and test
            int pos = i;
            if (pos > 1)
                pos += 6;
            bytes[pos / 8] ^= 1 << (pos % 8);

            uint16_t e = ccrc.calc(bytes, 7, 0);
            if (c == e)
            {
                fprintf(stderr, "False +ve %s\n", genMsg(bytes, sizeof(bytes)));
            }

            // flip bit i back again
            bytes[pos / 8] ^= 1 << (pos % 8);
        }
    }
}

void test_crc13_flip5(void)
{
    GENERIC_CRC13 ccrc = GENERIC_CRC13(ELRS_CRC13_POLY);

    for (int x = 0; x < NUM_ITERATIONS; x++)
    {
        uint8_t bytes[7];
        for (int i = 0; i < sizeof(bytes); i++)
            bytes[i] = random() % 255;
        bytes[0] &= 0b11;

        uint16_t c = ccrc.calc(bytes, 7, 0);

        // Flip 4 random bits
        for (int i = 0; i < 4; i++)
        {
            int pos = random() % 50;
            if (pos > 1) 
                pos += 6;
            bytes[pos / 8] ^= 1 << (pos % 8);
        }

        // Flip all the bits one after the other
        for (int i = 0; i < 50; i++)
        {
            // flip bit i and test
            int pos = i;
            if (pos > 1)
                pos += 6;
            bytes[pos / 8] ^= 1 << (pos % 8);

            uint16_t e = ccrc.calc(bytes, 7, 0);
            if (c == e)
            {
                fprintf(stderr, "False +ve %s\n", genMsg(bytes, sizeof(bytes)));
            }

            // flip bit i back again
            bytes[pos / 8] ^= 1 << (pos % 8);
        }
    }
}

void test_crc8(void)
{
    // Size of a CRSF packet
    uint8_t bytes[11];
    for (int i = 0; i < sizeof(bytes); i++)
        bytes[i] = random() % 255;

    uCRC_t ccrc = uCRC_t("CRC8", 8, ELRS_CRC_POLY, 0, false, false, 0);
    uint64_t crc = ccrc.get_raw_crc(bytes, 7, 0);

    GENERIC_CRC8 ecrc = GENERIC_CRC8(ELRS_CRC_POLY);
    uint16_t c = ecrc.calc(bytes, 7);

    TEST_ASSERT_EQUAL_MESSAGE((int)(crc & 0xFF), c, genMsg(bytes, sizeof(bytes)));
}

int main(int argc, char **argv)
{
    srandom(micros());

    UNITY_BEGIN();
    RUN_TEST(test_crc13_implementation_compatibility);
    RUN_TEST(test_crc13_flip5);
    RUN_TEST(test_crc8);
    UNITY_END();

    return 0;
}

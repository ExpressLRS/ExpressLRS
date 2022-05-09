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

void test_crc_implementation_compatibility(uint8_t crcbits, uint16_t poly, uint8_t testlen)
{
    uint8_t bytes[testlen];
    for (int i = 0; i < testlen; i++)
        bytes[i] = random() % 255;
    bytes[0] &= 0b11;

    uCRC_t ccrc = uCRC_t("CRC", crcbits, poly, 0, false, false, 0);
    uint64_t crc = ccrc.get_raw_crc(bytes, testlen, 0);

    Crc2Byte ecrc;
    ecrc.init(crcbits, poly);
    uint32_t c = ecrc.calc(bytes, testlen, 0);

    uint32_t mask = (1 << crcbits) - 1;
    TEST_ASSERT_EQUAL_MESSAGE(crc & mask, c, genMsg(bytes, testlen));
}

void test_crc14_implementation_compatibility(void)
{
    test_crc_implementation_compatibility(14, ELRS_CRC14_POLY, 7);
}

void test_crc16_implementation_compatibility(void)
{
    test_crc_implementation_compatibility(16, ELRS_CRC16_POLY, 11);
}

void test_crc_flip_random(uint8_t crcbits, uint16_t poly, uint8_t testlen, int flip)
{
    int false_positive = 0;
    Crc2Byte ccrc;
    ccrc.init(crcbits, poly);

    for (int x = 0; x < NUM_ITERATIONS; x++)
    {
        uint8_t bytes[testlen];
        uint8_t fbytes[testlen];

        for (int i = 0; i < testlen; i++)
            bytes[i] = random() % 255;
        bytes[0] &= 0b11;
        memcpy(fbytes, bytes, testlen);

        uint16_t c = ccrc.calc(bytes, testlen, 0);

        // Flip 'flip' random bits
        do {
            memcpy(bytes, fbytes, testlen);
            for (int i = 0; i < flip; i++)
            {
                int bit = random() % (testlen * 8);
                bytes[bit / 8] ^= 1 << (bit % 8);
            }
        } while (memcmp(fbytes, bytes, testlen) == 0);

        uint16_t e = ccrc.calc(bytes, testlen, 0);
        if (c == e)
        {
            //fprintf(stderr, "False +ve %s\n", genMsg(bytes, sizeof(bytes)));
            false_positive++;
        }
    }
    printf("%d out of %d false positives, %f%%\n", false_positive, NUM_ITERATIONS, false_positive * 100.0 / NUM_ITERATIONS);
}

void test_crc14_flip_random(int flip)
{
    test_crc_flip_random(14, ELRS_CRC14_POLY, 7, flip);
}

void test_crc16_flip_random(int flip)
{
    test_crc_flip_random(16, ELRS_CRC16_POLY, 11, flip);
}

void test_crc_flip_sequential(uint8_t crcbits, uint16_t poly, uint8_t testlen, int flip)
{
    int false_positive = 0;
    Crc2Byte ccrc;
    ccrc.init(crcbits, poly);

    for (int x=0 ; x<NUM_ITERATIONS ; x++) {
        uint8_t bytes[7];
        uint8_t fbytes[7];

        for (int i = 0; i < testlen; i++)
            bytes[i] = random() % 255;
        bytes[0] &= 0b11;

        memcpy(fbytes, bytes, testlen);

        uint16_t c = ccrc.calc(bytes, testlen, 0);

        // Flip 'flip' bits starting at a random position
        do {
            memcpy(bytes, fbytes, testlen);
            int pos = random() % ((testlen * 8 - 6) - flip);
            for(int bit=pos ; bit<pos+flip ; bit++) {
                int f = bit>1 ? bit+6 : bit;
                bytes[f / 8] ^= 1 << (f % 8);
            }
        } while (memcmp(fbytes, bytes, testlen) == 0);

        uint16_t e = ccrc.calc(bytes, testlen, 0);
        if (c == e)
        {
            //fprintf(stderr, "False +ve %s\n", genMsg(bytes, sizeof(bytes)));
            false_positive++;
        }
    }
    printf("%d out of %d false positives, %f%%\n", false_positive, NUM_ITERATIONS, false_positive*100.0/NUM_ITERATIONS);
}

void test_crc14_flip_sequential(int flip)
{
    test_crc_flip_sequential(14, ELRS_CRC14_POLY, 7, flip);
}

void test_crc16_flip_sequential(int flip)
{
    test_crc_flip_sequential(16, ELRS_CRC16_POLY, 11, flip);
}

void test_crc_flip_within(uint8_t crcbits, uint16_t poly, uint8_t testlen, int flip)
{
    int false_positive = 0;
    Crc2Byte ccrc;
    ccrc.init(crcbits, poly);

    for (int x = 0; x < NUM_ITERATIONS; x++)
    {
        uint8_t bytes[testlen];
        uint8_t fbytes[testlen];

        for (int i = 0; i < testlen; i++)
            bytes[i] = random() % 255;
        bytes[0] &= 0b11;

        memcpy(fbytes, bytes, testlen);

        uint16_t c = ccrc.calc(bytes, testlen, 0);

        // Flip 'flip' bits starting at a random position
        int start = random() % ((testlen * 8) - (crcbits - 1) - 6);
        do {
            memcpy(bytes, fbytes, testlen);
            for (int i = 0; i < flip; i++)
            {
                int bit = random() % (crcbits - 1) + start;
                if (bit>1) bit += 6;
                bytes[bit / 8] ^= 1 << (bit % 8);
            }
        } while (memcmp(fbytes, bytes, testlen)==0);

        uint16_t e = ccrc.calc(bytes, testlen, 0);
        if (c == e)
        {
            //fprintf(stderr, "False +ve %s\n", genMsg(bytes, sizeof(bytes)));
            false_positive++;
        }
    }
    printf("%d out of %d false positives, %f%%\n", false_positive, NUM_ITERATIONS, false_positive * 100.0 / NUM_ITERATIONS);
}

void test_crc14_flip_within(int flip)
{
    test_crc_flip_within(14, ELRS_CRC14_POLY, 7, flip);
}

void test_crc16_flip_within(int flip)
{
    test_crc_flip_within(16, ELRS_CRC16_POLY, 11, flip);
}

void test_crc_flip5(uint8_t crcbits, uint16_t poly, uint8_t testlen)
{
    Crc2Byte ccrc;
    ccrc.init(crcbits, poly);

    for (int x = 0; x < NUM_ITERATIONS; x++)
    {
        uint8_t bytes[testlen];
        uint8_t fbytes[testlen];

        for (int i = 0; i < testlen; i++)
            bytes[i] = random() % 255;
        bytes[0] &= 0b11;

        memcpy(fbytes, bytes, testlen);

        uint16_t c = ccrc.calc(bytes, testlen, 0);

        // Flip 4 random bits
        do {
            memcpy(bytes, fbytes, testlen);
            for (int i = 0; i < 4; i++)
            {
                int pos = random() % ((testlen * 8) - 6);
                if (pos > 1)
                    pos += 6;
                bytes[pos / 8] ^= 1 << (pos % 8);
            }
        }
        while (memcmp(fbytes, bytes, testlen) == 0);

    // Flip all the bits one after the other
    for (int i = 0; i < (testlen * 8) - 6; i++)
    {
        // flip bit i and test
        int pos = i;
        if (pos > 1)
            pos += 6;
        bytes[pos / 8] ^= 1 << (pos % 8);

        uint16_t e = ccrc.calc(bytes, testlen, 0);
        if (c == e && memcmp(fbytes, bytes, testlen) != 0)
        {
            fprintf(stderr, "False +ve %s\n", genMsg(bytes, sizeof(bytes)));
        }

        // flip bit i back again
        bytes[pos / 8] ^= 1 << (pos % 8);
        }
    }
}

void test_crc14_flip5(void)
{
    test_crc_flip5(14, ELRS_CRC14_POLY, 7);
}

void test_crc16_flip5(void)
{
    test_crc_flip5(16, ELRS_CRC16_POLY, 11);
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
#ifndef BIG_TEST
    UNITY_BEGIN();
    RUN_TEST(test_crc14_implementation_compatibility);
    RUN_TEST(test_crc14_flip5);
    RUN_TEST(test_crc16_implementation_compatibility);
    RUN_TEST(test_crc16_flip5);
    RUN_TEST(test_crc8);
    UNITY_END();
#endif
#ifdef BIG_TEST
    printf("Random flipped bits within 14-bit range\n");
    for (int i = 1; i < 31; i++)
    {
        printf("%2d : ", i);
        test_crc14_flip_within(i);
    }
    printf("============================================================\n");
    printf("Random flipped bits within 16-bit range\n");
    for (int i = 1; i < 31; i++)
    {
        printf("%2d : ", i);
        test_crc16_flip_within(i);
    }
    printf("============================================================\n");
    printf("Flipped bits in a single sequence at random start position\n");
    for (int i = 1; i < 31; i++)
    {
        printf("%2d : ", i);
        test_crc14_flip_sequential(i);
    }
    printf("============================================================\n");
    printf("Randomly flipped bits (14 bit)\n");
    for (int i = 1; i < 31; i++)
    {
        printf("%2d : ", i);
        test_crc14_flip_random(i);
    }
    printf("============================================================\n");
    printf("Randomly flipped bits (16 bit)\n");
    for (int i = 1; i < 31; i++)
    {
        printf("%2d : ", i);
        test_crc16_flip_random(i);
    }
    printf("============================================================\n");
#endif

    return 0;
}

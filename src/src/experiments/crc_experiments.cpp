#include <cstdint>
#include <crc.h>
#include "common.h"

#define NUM_ITERATIONS 1000000

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

void test_crc14_flip_random(int flip)
{
    int false_positive = 0;
    GENERIC_CRC14 ccrc = GENERIC_CRC14(ELRS_CRC14_POLY);

    for (int x = 0; x < NUM_ITERATIONS; x++)
    {
        uint8_t bytes[7];
        uint8_t fbytes[7];

        for (int i = 0; i < sizeof(bytes); i++)
            bytes[i] = random() % 255;
        bytes[0] &= 0b11;
        memcpy(fbytes, bytes, 7);

        uint16_t c = ccrc.calc(bytes, 7, 0);

        // Flip 'flip' random bits
        do {
            memcpy(bytes, fbytes, 7);
            for (int i = 0; i < flip; i++)
            {
                int bit = random() % 50;
                if (bit>1) bit+=6;
                bytes[bit / 8] ^= 1 << (bit % 8);
            }
        } while (memcmp(fbytes, bytes, 7) == 0);

        uint16_t e = ccrc.calc(bytes, 7, 0);
        if (c == e)
        {
            //fprintf(stderr, "False +ve %s\n", genMsg(bytes, sizeof(bytes)));
            false_positive++;
        }
    }
    printf("%d out of %d false positives, %f%%\n", false_positive, NUM_ITERATIONS, false_positive * 100.0 / NUM_ITERATIONS);
}

void test_crc14_flip_sequential(int flip)
{
    int false_positive = 0;
    GENERIC_CRC14 ccrc = GENERIC_CRC14(ELRS_CRC14_POLY);

    for (int x=0 ; x<NUM_ITERATIONS ; x++) {
        uint8_t bytes[7];
        uint8_t fbytes[7];

        for (int i = 0; i < sizeof(bytes); i++)
            bytes[i] = random() % 255;
        bytes[0] &= 0b11;

        memcpy(fbytes, bytes, 7);

        uint16_t c = ccrc.calc(bytes, 7, 0);

        // Flip 'flip' bits starting at a random position
        do {
            memcpy(bytes, fbytes, 7);
            int pos = random() % (50 - flip);
            for(int bit=pos ; bit<pos+flip ; bit++) {
                int f = bit>1 ? bit+6 : bit;
                bytes[f / 8] ^= 1 << (f % 8);
            }
        } while (memcmp(fbytes, bytes, 7) == 0);

        uint16_t e = ccrc.calc(bytes, 7, 0);
        if (c == e)
        {
            //fprintf(stderr, "False +ve %s\n", genMsg(bytes, sizeof(bytes)));
            false_positive++;
        }
    }
    printf("%d out of %d false positives, %f%%\n", false_positive, NUM_ITERATIONS, false_positive*100.0/NUM_ITERATIONS);
}

void test_crc14_flip_within(int flip)
{
    int false_positive = 0;
    GENERIC_CRC14 ccrc = GENERIC_CRC14(ELRS_CRC14_POLY);

    for (int x = 0; x < NUM_ITERATIONS; x++)
    {
        uint8_t bytes[7];
        uint8_t fbytes[7];

        for (int i = 0; i < sizeof(bytes); i++)
            bytes[i] = random() % 255;
        bytes[0] &= 0b11;

        memcpy(fbytes, bytes, 7);

        uint16_t c = ccrc.calc(bytes, 7, 0);

        // Flip 'flip' bits starting at a random position
        int start = random() % (50 - 13);
        do {
            memcpy(bytes, fbytes, 7);
            for (int i = 0; i < flip; i++)
            {
                int bit = random() % 13 + start;
                if (bit>1) bit += 6;
                bytes[bit / 8] ^= 1 << (bit % 8);
            }
        } while (memcmp(fbytes, bytes, 7)==0);

        uint16_t e = ccrc.calc(bytes, 7, 0);
        if (c == e)
        {
            //fprintf(stderr, "False +ve %s\n", genMsg(bytes, sizeof(bytes)));
            false_positive++;
        }
    }
    printf("%d out of %d false positives, %f%%\n", false_positive, NUM_ITERATIONS, false_positive * 100.0 / NUM_ITERATIONS);
}

void test_crc14_flip5(void)
{
    GENERIC_CRC14 ccrc = GENERIC_CRC14(ELRS_CRC14_POLY);

    for (int x = 0; x < NUM_ITERATIONS; x++)
    {
        uint8_t bytes[7];
        uint8_t fbytes[7];

        for (int i = 0; i < sizeof(bytes); i++)
            bytes[i] = random() % 255;
        bytes[0] &= 0b11;

        memcpy(fbytes, bytes, 7);

        uint16_t c = ccrc.calc(bytes, 7, 0);

        // Flip 4 random bits
        do {
            memcpy(bytes, fbytes, 7);
            for (int i = 0; i < 4; i++)
            {
                int pos = random() % 50;
                if (pos > 1) 
                    pos += 6;
                bytes[pos / 8] ^= 1 << (pos % 8);
            }
        }
        while (memcmp(fbytes, bytes, 7) == 0);

        // Flip all the bits one after the other
        for (int i = 0; i < 50; i++)
        {
            // flip bit i and test
            int pos = i;
            if (pos > 1)
                pos += 6;
            bytes[pos / 8] ^= 1 << (pos % 8);

            uint16_t e = ccrc.calc(bytes, 7, 0);
            if (c == e && memcmp(fbytes, bytes, 7) != 0)
            {
                fprintf(stderr, "False +ve %s\n", genMsg(bytes, sizeof(bytes)));
            }

            // flip bit i back again
            bytes[pos / 8] ^= 1 << (pos % 8);
        }
    }
}

int main(int argc, char **argv)
{
    srandom(micros());
    printf("Random flipped bits within 14-bit range\n");
    for (int i = 1; i < 31; i++)
    {
        printf("%2d : ", i);
        test_crc14_flip_within(i);
    }
    printf("============================================================\n");
    printf("Flipped bits in a single sequence at random start position\n");
    for (int i = 1; i < 31; i++)
    {
        printf("%2d : ", i);
        test_crc14_flip_sequential(i);
    }
    printf("============================================================\n");
    printf("Randomly flipped bits\n");
    for (int i = 1; i < 31; i++)
    {
        printf("%2d : ", i);
        test_crc14_flip_random(i);
    }
    printf("============================================================\n");
    printf("Flip 4 random bits then flip each other bit in sequqnce\n");
    for (int i = 1; i < 31; i++)
    {
        printf("%2d : ", i);
        test_crc14_flip_random(i);
    }
    printf("============================================================\n");

    return 0;
}

#include <cstdint>
#include <unity.h>
#include <crc.h>
#include "common.h"

#define NUM_ITERATIONS 100000

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

bool flip_random(int flip, uint8_t bytes[7])
{
    uint8_t fbytes[7];
    memcpy(fbytes, bytes, 7);
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
    return false;
}

bool flip_sequential(int flip, uint8_t bytes[7])
{
    uint8_t fbytes[7];
    memcpy(fbytes, bytes, 7);
    // Flip 'flip' bits starting at a random position
    do {
        memcpy(bytes, fbytes, 7);
        int pos = random() % (50 - flip);
        for(int bit=pos ; bit<pos+flip ; bit++) {
            int f = bit>1 ? bit+6 : bit;
            bytes[f / 8] ^= 1 << (f % 8);
        }
    } while (memcmp(fbytes, bytes, 7) == 0);
    return false;
}

bool flip_within(int flip, uint8_t bytes[7])
{
    uint8_t fbytes[7];
    memcpy(fbytes, bytes, 7);
    // Flip 'flip' bits starting at a random position
    int start = random() % (50 - 13);
    do {
        memcpy(bytes, fbytes, 7);
        for (int i = 0; i < flip; i++)
        {
            int bit = random() % 13 + start;
            if (bit>1) bit+=6;
            bytes[bit / 8] ^= 1 << (bit % 8);
        }
    } while (memcmp(fbytes, bytes, 7)==0);
    return false;
}

bool nonce_offset(int flip, uint8_t bytes[7])
{
    uint8_t nonce = (bytes[0] >> 2) & 0b11;
    nonce++;
    bytes[0] = (bytes[0] & ~0b1100) | ((nonce & 0xb11) << 2);
    return true;
}

void test_crc14(int flip, bool (*perturb)(int, uint8_t bytes[7]))
{
    GENERIC_CRC14 ccrc = GENERIC_CRC14(ELRS_CRC14_POLY);

    for(int nonce=0 ; nonce<4 ; nonce++)
    {
        int false_positive = 0;
        int positive_on[4] = {0,0,0,0};
        for (int x = 0; x < NUM_ITERATIONS; x++)
        {
            uint8_t bytes[7];

            for (int i = 0; i < sizeof(bytes); i++)
                bytes[i] = random() % 255;
            
            // reset crc-bits and keep packet type
            bytes[0] &= 0b11;
            // set expected nonce
            bytes[0] |= nonce << 2;

            uint16_t c = ccrc.calc(bytes, 7, 0);

            if(!perturb(flip, bytes)) {
                // reset crc-bits and keep packet type
                bytes[0] &= 0b11;
                // set expected nonce
                bytes[0] |= nonce << 2;
            }

            uint16_t e = ccrc.calc(bytes, 7, 0);
            if (c == e)
            {
                //fprintf(stderr, "False +ve %s\n", genMsg(bytes, sizeof(bytes)));
                false_positive++;
            }
            else {
                for (int trynonce=0 ; trynonce<4 ; trynonce++)
                {
                    bytes[0] &= 0b11;
                    bytes[0] |= trynonce << 2;
                    uint16_t e = ccrc.calc(bytes, 7, 0);
                    if (c==e)
                    {
                        positive_on[trynonce]++;
                    }
                }
            }
        }
        printf("expected nonce %d, %d out of %d false positives (%f%%) : ", nonce, false_positive, NUM_ITERATIONS, false_positive * 100.0 / NUM_ITERATIONS);
        for (int trynonce=0 ; trynonce<4 ; trynonce++) {
            printf("%d (%f%%)  ", positive_on[trynonce], positive_on[trynonce] * 100.0 / NUM_ITERATIONS);
        }
        printf("\n");
    }
}

int main(int argc, char **argv)
{
    srandom(micros());
    printf("Random flipped bits within 14-bit range\n");
    for (int i = 1; i < 31; i++)
    {
        printf("%2d:\n", i);
        test_crc14(i, flip_within);
    }
    printf("============================================================\n");
    printf("Flipped bits in a single sequence at random start position\n");
    for (int i = 1; i < 31; i++)
    {
        printf("%2d:\n", i);
        test_crc14(i, flip_sequential);
    }
    printf("============================================================\n");
    printf("Randomly flipped bits\n");
    for (int i = 1; i < 31; i++)
    {
        printf("%2d:\n", i);
        test_crc14(i, flip_random);
    }
    printf("============================================================\n");
    printf("Nonce offset\n");
    for (int i = 1; i < 31; i++)
    {
        printf("%2d:\n", i);
        test_crc14(i, nonce_offset);
    }
    printf("============================================================\n");

    return 0;
}

#include <cstdint>
#include <iostream>
#include <unity.h>
#include "common.h"
#include "msp2crsf.h"
#include "crsf2msp.h"

using namespace std;

GENERIC_CRC8 crsf_crc(CRSF_CRC_POLY);

MSP2CROSSFIRE msp2crsf;
CROSSFIRE2MSP crsf2msp;

// MSP V2 (function id: 100, payload size: 0)
const uint8_t MSP_IDENT[] = {0x24, 0x58, 0x3c, 0x00, 0x64, 0x00, 0x00, 0x00, 0x8f};
const uint8_t MSPV2_HELLO_WORLD[] = {0x24, 0x58, 0x3e, 0xa5, 0x42, 0x42, 0x12, 0x00, 0x48, 0x65, 0x6c, 0x6c, 0x6f, 0x20, 0x66, 0x6c, 0x79, 0x69, 0x6e, 0x67, 0x20, 0x77, 0x6f, 0x72, 0x6c, 0x64, 0x82};
const uint8_t MSPV2_IN_V1_HELLOWORLD[] = {0x24, 0x4d, 0x3e, 0x18, 0xff, 0xa5, 0x42, 0x42, 0x12, 0x00, 0x48, 0x65, 0x6c, 0x6c, 0x6f, 0x20, 0x66, 0x6c, 0x79, 0x69, 0x6e, 0x67, 0x20, 0x77, 0x6f, 0x72, 0x6c, 0x64, 0x82, 0xe1};
const uint8_t MSP_2CHUNKS_LONG[] = {36, 77, 62, 75, 4, 83, 52, 48, 53, 0, 0, 2, 55, 9, 83, 84, 77, 51, 50, 70, 52, 48, 53, 9, 79, 77, 78, 73, 66, 85, 83, 70, 52, 4, 65, 73, 82, 66, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 2, 64, 31, 3, 0, 0, 0, 1, 0, 87};

void printBufferhex(const uint8_t *buf, int len)
{
    cout << "len: " << dec << (int)len << " [ ";
    for (int i = 0; i < len; i++)
    {
        cout << "0x" << hex << (int)buf[i] << " ";
    }
    cout << "]" << endl;
}

void printFIFOhex()
{
    while (msp2crsf.FIFOout.peek())
    {
        uint8_t len = msp2crsf.FIFOout.pop();
        cout << "len: " << dec << (int)len << " [ ";

        for (int i = 0; i < len; i++)
        {
            cout << "0x";
            cout << hex << (int)msp2crsf.FIFOout.pop() << " ";
        }
        cout << "]" << endl;
    }
}

void runTestChunked(const uint8_t *frame, int frameLen)
{
    // cout << "MSP()                ";
    // printBufferhex(frame, frameLen);

    // msp2crsf.parse(frame, frameLen);
    // cout << "CRSF(MSP())          ";
    // printFIFOhex();

    msp2crsf.parse(frame, frameLen); // do again cause we pop'd the buffer
    while (msp2crsf.FIFOout.peek() > 0)
    {
        uint8_t sizeOut = msp2crsf.FIFOout.pop();
        uint8_t crsfFrame[64];
        msp2crsf.FIFOout.popBytes(crsfFrame, sizeOut);
        crsf2msp.parse(crsfFrame, sizeOut);
    }

    // if (crsf2msp.isFrameReady())
    // {
    //     cout << "CRSF^-1(CRSF(MSP())) ";
    //     printBufferhex(crsf2msp.getFrame(), crsf2msp.getFrameLen());
    // }
    // else
    // {
    //     cout << "Frame not ready\n";
    // }
    TEST_ASSERT_EQUAL_HEX8_ARRAY(frame, crsf2msp.getFrame(), frameLen);
}

void runTest(const uint8_t *frame, int frameLen)
{
    // msp2crsf.parse(frame, frameLen);
    // cout << "MSP()                ";
    // printBufferhex(frame, frameLen);
    // cout << "CRSF(MSP())          ";
    // printFIFOhex();

    msp2crsf.parse(frame, frameLen);
    
    uint8_t sizeOut = msp2crsf.FIFOout.pop();
    uint8_t crsfFrame[64];
    msp2crsf.FIFOout.popBytes(crsfFrame, sizeOut);

    crsf2msp.parse(crsfFrame, sizeOut);
    // if (crsf2msp.isFrameReady())
    // {
    //     cout << "CRSF^-1(CRSF(MSP())) ";
    //     printBufferhex(crsf2msp.getFrame(), crsf2msp.getFrameLen());
    // }
    // else
    // {
    //     cout << "Frame not ready\n";
    // }
    TEST_ASSERT_EQUAL_HEX8_ARRAY(frame, crsf2msp.getFrame(), frameLen);
}

void MSP_IDENT_TEST()
{
    // cout << "Testing MSP_IDENT (0 len payload)" << endl;
    runTest(MSP_IDENT, sizeof(MSP_IDENT));
    // cout << endl;
}

void MSPV2_HELLO_WORLD_TEST()
{
    // cout << "Testing MSPV2 Hello World" << endl;
    runTest(MSPV2_HELLO_WORLD, sizeof(MSPV2_HELLO_WORLD));
    // cout << endl;
}

void MSPV2_IN_V1_HELLOWORLD_TEST()
{
    // cout << "Testing MSPV2 within MSPV1 Hello World" << endl;
    runTest(MSPV2_IN_V1_HELLOWORLD, sizeof(MSPV2_IN_V1_HELLOWORLD));
    // cout << endl;
}

void MSPV2_2_CHUNK_MSG()
{
    // cout << "Testing MSP LONG" << endl;
    runTestChunked(MSP_2CHUNKS_LONG, sizeof(MSP_2CHUNKS_LONG));
    // cout << endl;
}

main(int argc, char **argv)
{
    UNITY_BEGIN();

    RUN_TEST(MSP_IDENT_TEST);
    RUN_TEST(MSPV2_HELLO_WORLD_TEST);
    RUN_TEST(MSPV2_IN_V1_HELLOWORLD_TEST);
    RUN_TEST(MSPV2_IN_V1_HELLOWORLD_TEST);
    RUN_TEST(MSPV2_2_CHUNK_MSG);

    UNITY_END();

    return 0;
}

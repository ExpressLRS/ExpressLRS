#include <cstdint>
#include <iostream>
#include <unity.h>
#include "common.h"
#include "msp2crsf.h"
#include "crsf2msp.h"
#include "CRSFEndpoint.h"

using namespace std;

GENERIC_CRC8 crsf_crc(CRSF_CRC_POLY);

MSP2CROSSFIRE msp2crsf;
CROSSFIRE2MSP crsf2msp;

class MockEndpoint : public CRSFEndpoint
{
public:
    MockEndpoint() : CRSFEndpoint((crsf_addr_e)1) {}
    bool handleMessage(const crsf_header_t *message) override { return false; }
};
CRSFEndpoint *crsfEndpoint = new MockEndpoint();

// MSP V2 (function id: 100, payload size: 0)
const uint8_t MSP_IDENT[] = {0x24, 0x58, 0x3c, 0x00, 0x64, 0x00, 0x00, 0x00, 0x8f};
const uint8_t MSPV2_HELLO_WORLD[] = {0x24, 0x58, 0x3e, 0xa5, 0x42, 0x42, 0x12, 0x00, 0x48, 0x65, 0x6c, 0x6c, 0x6f, 0x20, 0x66, 0x6c, 0x79, 0x69, 0x6e, 0x67, 0x20, 0x77, 0x6f, 0x72, 0x6c, 0x64, 0x82};
const uint8_t MSPV2_IN_V1_HELLOWORLD[] = {0x24, 0x4d, 0x3e, 0x18, 0xff, 0xa5, 0x42, 0x42, 0x12, 0x00, 0x48, 0x65, 0x6c, 0x6c, 0x6f, 0x20, 0x66, 0x6c, 0x79, 0x69, 0x6e, 0x67, 0x20, 0x77, 0x6f, 0x72, 0x6c, 0x64, 0x82, 0xe1};
const uint8_t MSP_2CHUNKS_LONG[] = {36, 77, 62, 75, 4, 83, 52, 48, 53, 0, 0, 2, 55, 9, 83, 84, 77, 51, 50, 70, 52, 48, 53, 9, 79, 77, 78, 73, 66, 85, 83, 70, 52, 4, 65, 73, 82, 66, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 2, 64, 31, 3, 0, 0, 0, 1, 0, 87};
const uint8_t MSPV1_JUMBO[] = {36, 77, 62, 255, 116, 25, 1, 65, 82, 77, 59, 65, 78, 71, 76, 69, 59, 72, 79, 82, 73, 90, 79, 78, 59, 72, 69, 65, 68, 70, 82, 69, 69, 59, 70, 65, 73, 76, 83, 65, 70, 69, 59, 72, 69, 65, 68, 65, 68, 74, 59, 66, 69, 69, 80, 69, 82, 59, 79, 83, 68, 32, 68, 73, 83, 65, 66, 76, 69, 59, 84, 69, 76, 69, 77, 69, 84, 82, 89, 59, 66, 76, 65, 67, 75, 66, 79, 88, 59, 70, 80, 86, 32, 65, 78, 71, 76, 69, 32, 77, 73, 88, 59, 66, 76, 65, 67, 75, 66, 79, 88, 32, 69, 82, 65, 83, 69, 32, 40, 62, 51, 48, 115, 41, 59, 67, 65, 77, 69, 82, 65, 32, 67, 79, 78, 84, 82, 79, 76, 32, 49, 59, 67, 65, 77, 69, 82, 65, 32, 67, 79, 78, 84, 82, 79, 76, 32, 50, 59};

// sendt at near the start of the transaction, got stuck here for abit
const uint8_t MSPV1_81[] = {36, 77, 62, 75, 4, 83, 52, 48, 53, 0, 0, 2, 55, 9, 83, 84, 77, 51, 50, 70, 52, 48, 53, 9, 79, 77, 78, 73, 66, 85, 83, 70, 52, 4, 65, 73, 82, 66, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 2, 64, 31, 3, 0, 0, 0, 1, 0, 87};

// MSPv1JUMBO
const uint8_t MSPV1_JUMBO_289[] = {36, 77, 62, 255, 116, 25, 1, 65, 82, 77, 59, 65, 78, 71, 76, 69, 59, 72, 79, 82, 73, 90, 79, 78, 59, 72, 69, 65, 68, 70, 82, 69, 69, 59, 70, 65, 73, 76, 83, 65, 70, 69, 59, 72, 69, 65, 68, 65, 68, 74, 59, 66, 69, 69, 80, 69, 82, 59, 79, 83, 68, 32, 68, 73, 83, 65, 66, 76, 69, 59, 84, 69, 76, 69, 77, 69, 84, 82, 89, 59, 66, 76, 65, 67, 75, 66, 79, 88, 59, 70, 80, 86, 32, 65, 78, 71, 76, 69, 32, 77, 73, 88, 59, 66, 76, 65, 67, 75, 66, 79, 88, 32, 69, 82, 65, 83, 69, 32, 40, 62, 51, 48, 115, 41, 59, 67, 65, 77, 69, 82, 65, 32, 67, 79, 78, 84, 82, 79, 76, 32, 49, 59, 67, 65, 77, 69, 82, 65, 32, 67, 79, 78, 84, 82, 79, 76, 32, 50, 59, 67, 65, 77, 69, 82, 65, 32, 67, 79, 78, 84, 82, 79, 76, 32, 51, 59, 80, 82, 69, 65, 82, 77, 59, 86, 84, 88, 32, 80, 73, 84, 32, 77, 79, 68, 69, 59, 80, 65, 82, 65, 76, 89, 90, 69, 59, 65, 67, 82, 79, 32, 84, 82, 65, 73, 78, 69, 82, 59, 86, 84, 88, 32, 67, 79, 78, 84, 82, 79, 76, 32, 68, 73, 83, 65, 66, 76, 69, 59, 76, 65, 85, 78, 67, 72, 32, 67, 79, 78, 84, 82, 79, 76, 59, 83, 84, 73, 67, 75, 32, 67, 79, 77, 77, 65, 78, 68, 83, 32, 68, 73, 83, 65, 66, 76, 69, 59, 66, 69, 69, 80, 69, 82, 32, 77, 85, 84, 69, 59, 150};

const uint8_t MSP_BOARD_INFO_81[] = {36, 77, 62, 75, 4, 83, 52, 48, 53, 0, 0, 2, 55, 9, 83, 84, 77, 51, 50, 70, 52, 48, 53, 9, 79, 77, 78, 73, 66, 85, 83, 70, 52, 4, 65, 73, 82, 66, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

// MSPV2 46b
const uint8_t MSPV2_SERIAL_SETTINGS[] = {0x24, 0x58, 0x3C, 0x00, 0x0A, 0x10, 0x25, 0x00, 0x04, 0x14, 0x01, 0x00, 0x00, 0x00, 0x05, 0x04, 0x00, 0x05, 0x00, 0x40, 0x00, 0x00, 0x00, 0x05, 0x04, 0x00, 0x05, 0x02, 0x00, 0x00, 0x00, 0x00, 0x05, 0x04, 0x00, 0x05, 0x05, 0x00, 0x00, 0x00, 0x00, 0x05, 0x04, 0x00, 0x05, 0x7B};

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

void runTest(const uint8_t *frame, int frameLen)
{
    cout << "MSP In Len: " << dec << (int)frameLen << endl;

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
        crsf2msp.parse(crsfFrame);
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
    cout << "MSP Out Len: " << dec << (int)crsf2msp.getFrameLen() << endl;
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
    runTest(MSP_2CHUNKS_LONG, sizeof(MSP_2CHUNKS_LONG));
    // cout << endl;
}

void MSPV1_JUMBO_TEST()
{
    // cout << "Testing MSPV2 within MSPV1 Hello World" << endl;
    runTest(MSPV1_JUMBO, sizeof(MSPV1_JUMBO));
    // cout << endl;
}

void MSPV1_81_TEST()
{
    // cout << "Testing MSPV2 within MSPV1 Hello World" << endl;
    runTest(MSPV1_81, sizeof(MSPV1_81));
    // cout << endl;
}

void MSPV1_JUMBO_289_TEST()
{
    // cout << "Testing MSPV2 within MSPV1 Hello World" << endl;
    runTest(MSPV1_JUMBO_289, sizeof(MSPV1_JUMBO_289));
    // cout << endl;
}

void MSP_BOARD_INFO_81_TEST()
{
    // cout << "Testing MSPV2 within MSPV1 Hello World" << endl;
    runTest(MSP_BOARD_INFO_81, sizeof(MSP_BOARD_INFO_81));
    // cout << endl;
}

void MSPV2_SERIAL_SETTINGS_TEST()
{
    // cout << "Testing MSPV2 within MSPV1 Hello World" << endl;
    runTest(MSPV2_SERIAL_SETTINGS, sizeof(MSPV2_SERIAL_SETTINGS));
    // cout << endl;
}

// Unity setup/teardown
void setUp() {}
void tearDown() {}

int main(int argc, char **argv)
{
    UNITY_BEGIN();

    RUN_TEST(MSP_IDENT_TEST);
    RUN_TEST(MSPV2_HELLO_WORLD_TEST);
    RUN_TEST(MSPV2_IN_V1_HELLOWORLD_TEST);
    RUN_TEST(MSPV2_2_CHUNK_MSG);
    RUN_TEST(MSPV1_JUMBO_TEST);
    RUN_TEST(MSPV1_81_TEST);
    RUN_TEST(MSPV1_JUMBO_289_TEST);
    RUN_TEST(MSP_BOARD_INFO_81_TEST);
    RUN_TEST(MSPV2_SERIAL_SETTINGS_TEST);

    UNITY_END();

    return 0;
}

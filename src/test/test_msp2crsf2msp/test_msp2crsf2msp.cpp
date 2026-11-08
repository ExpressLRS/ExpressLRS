#include "CRSFRouter.h"
#include "common.h"
#include "crsf2msp.h"
#include "msp2crsf.h"
#include <cstring>
#include <iostream>
#include <unity.h>

using namespace std;

GENERIC_CRC8 crsf_crc(CRSF_CRC_POLY);

MSP2CROSSFIRE msp2crsf;
CROSSFIRE2MSP crsf2msp;

CRSFRouter crsfRouter;
class MockConnector : public CRSFConnector
{
public:
    MockConnector()
    {
        addDevice(CRSF_ADDRESS_FLIGHT_CONTROLLER);
        crsfRouter.addConnector(this);
    }

    uint32_t forwardCount = 0;

    void forwardMessage(const crsf_header_t *message) override
    {
        forwardCount++;
        crsf2msp.parse((uint8_t *)message, [](const uint8_t *, const uint32_t){});
    }
} crsfConnector;

// MSP V2 (function id: 100, payload size: 0)
const uint8_t MSP_IDENT[] = {0x24, 0x58, 0x3c, 0x00, 0x64, 0x00, 0x00, 0x00, 0x8f};
const uint8_t MSPV2_HELLO_WORLD[] = {0x24, 0x58, 0x3e, 0xa5, 0x42, 0x42, 0x12, 0x00, 0x48, 0x65, 0x6c, 0x6c, 0x6f, 0x20, 0x66, 0x6c, 0x79, 0x69, 0x6e, 0x67, 0x20, 0x77, 0x6f, 0x72, 0x6c, 0x64, 0x82};
const uint8_t MSPV2_IN_V1_HELLOWORLD[] = {0x24, 0x4d, 0x3e, 0x18, 0xff, 0xa5, 0x42, 0x42, 0x12, 0x00, 0x48, 0x65, 0x6c, 0x6c, 0x6f, 0x20, 0x66, 0x6c, 0x79, 0x69, 0x6e, 0x67, 0x20, 0x77, 0x6f, 0x72, 0x6c, 0x64, 0x82, 0xe1};
const uint8_t MSP_2CHUNKS_LONG[] = {36, 77, 62, 75, 4, 83, 52, 48, 53, 0, 0, 2, 55, 9, 83, 84, 77, 51, 50, 70, 52, 48, 53, 9, 79, 77, 78, 73, 66, 85, 83, 70, 52, 4, 65, 73, 82, 66, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 2, 64, 31, 3, 0, 0, 0, 1, 0, 87};
// MSPv1 JUMBO, truncated: header declares 281 payload bytes (289 byte frame) but only 159 bytes are present
const uint8_t MSPV1_JUMBO_TRUNCATED[] = {36, 77, 62, 255, 116, 25, 1, 65, 82, 77, 59, 65, 78, 71, 76, 69, 59, 72, 79, 82, 73, 90, 79, 78, 59, 72, 69, 65, 68, 70, 82, 69, 69, 59, 70, 65, 73, 76, 83, 65, 70, 69, 59, 72, 69, 65, 68, 65, 68, 74, 59, 66, 69, 69, 80, 69, 82, 59, 79, 83, 68, 32, 68, 73, 83, 65, 66, 76, 69, 59, 84, 69, 76, 69, 77, 69, 84, 82, 89, 59, 66, 76, 65, 67, 75, 66, 79, 88, 59, 70, 80, 86, 32, 65, 78, 71, 76, 69, 32, 77, 73, 88, 59, 66, 76, 65, 67, 75, 66, 79, 88, 32, 69, 82, 65, 83, 69, 32, 40, 62, 51, 48, 115, 41, 59, 67, 65, 77, 69, 82, 65, 32, 67, 79, 78, 84, 82, 79, 76, 32, 49, 59, 67, 65, 77, 69, 82, 65, 32, 67, 79, 78, 84, 82, 79, 76, 32, 50, 59};

// sendt at near the start of the transaction, got stuck here for abit
const uint8_t MSPV1_81[] = {36, 77, 62, 75, 4, 83, 52, 48, 53, 0, 0, 2, 55, 9, 83, 84, 77, 51, 50, 70, 52, 48, 53, 9, 79, 77, 78, 73, 66, 85, 83, 70, 52, 4, 65, 73, 82, 66, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 2, 64, 31, 3, 0, 0, 0, 1, 0, 87};

// MSPv1JUMBO
const uint8_t MSPV1_JUMBO_289[] = {36, 77, 62, 255, 116, 25, 1, 65, 82, 77, 59, 65, 78, 71, 76, 69, 59, 72, 79, 82, 73, 90, 79, 78, 59, 72, 69, 65, 68, 70, 82, 69, 69, 59, 70, 65, 73, 76, 83, 65, 70, 69, 59, 72, 69, 65, 68, 65, 68, 74, 59, 66, 69, 69, 80, 69, 82, 59, 79, 83, 68, 32, 68, 73, 83, 65, 66, 76, 69, 59, 84, 69, 76, 69, 77, 69, 84, 82, 89, 59, 66, 76, 65, 67, 75, 66, 79, 88, 59, 70, 80, 86, 32, 65, 78, 71, 76, 69, 32, 77, 73, 88, 59, 66, 76, 65, 67, 75, 66, 79, 88, 32, 69, 82, 65, 83, 69, 32, 40, 62, 51, 48, 115, 41, 59, 67, 65, 77, 69, 82, 65, 32, 67, 79, 78, 84, 82, 79, 76, 32, 49, 59, 67, 65, 77, 69, 82, 65, 32, 67, 79, 78, 84, 82, 79, 76, 32, 50, 59, 67, 65, 77, 69, 82, 65, 32, 67, 79, 78, 84, 82, 79, 76, 32, 51, 59, 80, 82, 69, 65, 82, 77, 59, 86, 84, 88, 32, 80, 73, 84, 32, 77, 79, 68, 69, 59, 80, 65, 82, 65, 76, 89, 90, 69, 59, 65, 67, 82, 79, 32, 84, 82, 65, 73, 78, 69, 82, 59, 86, 84, 88, 32, 67, 79, 78, 84, 82, 79, 76, 32, 68, 73, 83, 65, 66, 76, 69, 59, 76, 65, 85, 78, 67, 72, 32, 67, 79, 78, 84, 82, 79, 76, 59, 83, 84, 73, 67, 75, 32, 67, 79, 77, 77, 65, 78, 68, 83, 32, 68, 73, 83, 65, 66, 76, 69, 59, 66, 69, 69, 80, 69, 82, 32, 77, 85, 84, 69, 59, 150};

// truncated: header declares 75 payload bytes (81 byte frame) but only 61 bytes are present
const uint8_t MSP_BOARD_INFO_81_TRUNCATED[] = {36, 77, 62, 75, 4, 83, 52, 48, 53, 0, 0, 2, 55, 9, 83, 84, 77, 51, 50, 70, 52, 48, 53, 9, 79, 77, 78, 73, 66, 85, 83, 70, 52, 4, 65, 73, 82, 66, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

// MSPV2 46b
const uint8_t MSPV2_SERIAL_SETTINGS[] = {0x24, 0x58, 0x3C, 0x00, 0x0A, 0x10, 0x25, 0x00, 0x04, 0x14, 0x01, 0x00, 0x00, 0x00, 0x05, 0x04, 0x00, 0x05, 0x00, 0x40, 0x00, 0x00, 0x00, 0x05, 0x04, 0x00, 0x05, 0x02, 0x00, 0x00, 0x00, 0x00, 0x05, 0x04, 0x00, 0x05, 0x05, 0x00, 0x00, 0x00, 0x00, 0x05, 0x04, 0x00, 0x05, 0x7B};

void runTest(const uint8_t *frame, const int frameLen)
{
    cout << "MSP In Len: " << dec << (int)frameLen << endl;

    // cout << "MSP()                ";
    // printBufferhex(frame, frameLen);

    // msp2crsf.parse(frame, frameLen);
    // cout << "CRSF(MSP())          ";
    // printFIFOhex();

    msp2crsf.parse(nullptr, frame, frameLen); // do again because we pop'd the buffer

    // if (crsf2msp.isFrameReady())
    // {
    //     cout << "CRSF^-1(CRSF(MSP())) ";
    //     printBufferhex(crsf2msp.getFrame(), crsf2msp.getFrameLen());
    // }
    // else
    // {
    //     cout << "Frame not ready\n";
    // }
    cout << "MSP Out Len: " << dec << crsf2msp.getFrameLen() << endl;
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

// Copies the first len bytes of frame into an exactly sized heap buffer (so ASan catches any
// read past len) and returns how many CRSF chunks msp2crsf emitted for it
static uint32_t parseExact(const uint8_t *frame, const uint32_t len)
{
    uint8_t *buf = new uint8_t[len];
    memcpy(buf, frame, len);
    crsfConnector.forwardCount = 0;
    msp2crsf.parse(nullptr, buf, len);
    delete[] buf;
    return crsfConnector.forwardCount;
}

void MSP2CRSF_TRUNCATED_REJECT_TEST()
{
    // frames shorter than what their header declares must not be sent (and not read past the end)
    TEST_ASSERT_EQUAL_UINT32(0, parseExact(MSPV1_JUMBO_TRUNCATED, sizeof(MSPV1_JUMBO_TRUNCATED)));
    TEST_ASSERT_EQUAL_UINT32(0, parseExact(MSP_BOARD_INFO_81_TRUNCATED, sizeof(MSP_BOARD_INFO_81_TRUNCATED)));
    // the complete frames are still sent
    TEST_ASSERT_NOT_EQUAL_UINT32(0, parseExact(MSPV1_JUMBO_289, sizeof(MSPV1_JUMBO_289)));
    TEST_ASSERT_NOT_EQUAL_UINT32(0, parseExact(MSPV1_81, sizeof(MSPV1_81)));
}

void MSP2CRSF_SHORT_FRAME_REJECT_TEST()
{
    // every proper prefix of a valid frame, including the bare/partial header, must be rejected
    const struct { const uint8_t *frame; uint32_t len; } frames[] = {
        {MSP_IDENT, sizeof(MSP_IDENT)},                         // v2
        {MSPV1_81, sizeof(MSPV1_81)},                           // v1
        {MSPV1_JUMBO_289, sizeof(MSPV1_JUMBO_289)},             // v1 jumbo
    };
    for (const auto &f : frames)
    {
        for (uint32_t len = 0; len < f.len - 1; len++) // len - 1: frame without the MSP crc
        {
            TEST_ASSERT_EQUAL_UINT32_MESSAGE(0, parseExact(f.frame, len), "prefix of valid frame was sent");
        }
    }
}

void MSP2CRSF_LARGE_V2_FRAME_TEST()
{
    // v2 frame with the maximum payload length of 65535: 65540 bytes after the 3 byte header,
    // needs 65540 / 57 + 1 = 1150 chunks, more than an uint8_t chunk counter can hold
    constexpr uint32_t payloadLen = 0xffff;
    constexpr uint32_t frameLen = 3 + 5 + payloadLen + 1;
    uint8_t *frame = new uint8_t[frameLen]();
    const uint8_t header[] = {'$', 'X', '<', 0x00, 0x64, 0x00, payloadLen & 0xff, payloadLen >> 8};
    memcpy(frame, header, sizeof(header));

    crsfConnector.forwardCount = 0;
    msp2crsf.parse(nullptr, frame, frameLen);
    delete[] frame;
    TEST_ASSERT_EQUAL_UINT32((5 + payloadLen) / CRSF_MSP_MAX_BYTES_PER_CHUNK + 1, crsfConnector.forwardCount);
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

void MSPV2_SERIAL_SETTINGS_TEST()
{
    // cout << "Testing MSPV2 within MSPV1 Hello World" << endl;
    runTest(MSPV2_SERIAL_SETTINGS, sizeof(MSPV2_SERIAL_SETTINGS));
    // cout << endl;
}

// A CRSF(MSP) chunk whose V2 header declares a 600 byte MSP payload, larger than outBuffer can hold
// [sync][frame_size][type][dest][src][status][payload...]
// status: bit4 = new frame, bits5-6 = version (2), bits0-3 = seq
const uint8_t CRSF_MSP_V2_OVERSIZED[] = {0xc8, 0x0a, 0x7a, 0xc8, 0xea, 0x50, 0x00, 0x64, 0x00, 0x58, 0x02};
// Same shape for a V1 JUMBO header declaring a 600 byte payload (payload: [0xff][cmd][size lo][size hi])
const uint8_t CRSF_MSP_JUMBO_OVERSIZED[] = {0xc8, 0x0a, 0x7a, 0xc8, 0xea, 0x30, 0xff, 0x74, 0x58, 0x02, 0x00};
// A continuation chunk (no new-frame bit, seq 1) with 57 bytes of payload
const uint8_t CRSF_MSP_CONTINUATION[] = {0xc8, 0x3e, 0x7a, 0xc8, 0xea, 0x41,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

static bool overflowFrameEmitted;
static void overflowRejectTest(const uint8_t *newFrameChunk)
{
    overflowFrameEmitted = false;
    const auto onFrame = [](uint8_t *, uint32_t) { overflowFrameEmitted = true; };

    crsf2msp.parse(newFrameChunk, onFrame);
    TEST_ASSERT_FALSE(overflowFrameEmitted);

    // continuation chunks after the reject must be ignored, not assembled into a bogus frame,
    // and must not advance the write index (getFrameLen() is idx + 1, so a reset parser reads 1)
    crsf2msp.parse(CRSF_MSP_CONTINUATION, onFrame);
    crsf2msp.parse(CRSF_MSP_CONTINUATION, onFrame);
    TEST_ASSERT_FALSE(overflowFrameEmitted);
    TEST_ASSERT_EQUAL_UINT32(1, crsf2msp.getFrameLen());

    // and the parser must recover: a valid frame afterwards round-trips normally
    runTest(MSP_IDENT, sizeof(MSP_IDENT));
}

void MSPV2_OVERSIZED_REJECT_TEST()
{
    overflowRejectTest(CRSF_MSP_V2_OVERSIZED);
}

void MSPV1_JUMBO_OVERSIZED_REJECT_TEST()
{
    overflowRejectTest(CRSF_MSP_JUMBO_OVERSIZED);
}

void CRSF_MSP_ERROR_REPLY_TEST()
{
    // The FC answers an unsupported command with an MSP error reply, which
    // arrives with the error bit set in the chunk status byte. It must be
    // reconstructed as '$M!' and delivered -- dropping it starves the client
    // of the answer, which reads as a dead command and (via probe timeouts)
    // disables whole configurator features. Status 0xB0 = error | newframe |
    // v1 | seq 0; chunk = [size 0][cmd 88].
    const uint8_t crsfErr[] = {0xC8, 7, 0x7B, 0x12, 0xC8, 0xB0, 0x00, 0x58, 0x00};
    const uint8_t expect[] = {'$', 'M', '!', 0x00, 0x58, 0x58};
    bool delivered = false;
    crsf2msp.parse(crsfErr, [&delivered](const uint8_t *, const uint32_t) { delivered = true; });
    TEST_ASSERT_TRUE(delivered);
    TEST_ASSERT_EQUAL_UINT32(sizeof(expect), crsf2msp.getFrameLen());
    TEST_ASSERT_EQUAL_HEX8_ARRAY(expect, crsf2msp.getFrame(), sizeof(expect));
}

// Feeds a CRSF(MSP) chunk to crsf2msp from an exactly sized heap buffer (frame_size + 2 bytes,
// so ASan catches reads past the CRSF frame) and returns whether an MSP frame was emitted
static bool crsf2mspExact(const uint8_t *chunk)
{
    const uint32_t len = chunk[1] + 2;
    uint8_t *buf = new uint8_t[len];
    memcpy(buf, chunk, len);
    bool emitted = false;
    crsf2msp.parse(buf, [&emitted](const uint8_t *, const uint32_t) { emitted = true; });
    delete[] buf;
    return emitted;
}

void CRSF2MSP_SHORT_CHUNK_REJECT_TEST()
{
    // [sync][frame_size][type][dest][src][status][payload...][crc]
    // status 0x30 = v1 | new frame | seq 0, payload would be [len 0][cmd 0x64]
    for (uint8_t frameSize = 2; frameSize <= 4; frameSize++)
    {
        // frame too short to even contain the status byte
        const uint8_t chunk[] = {0xc8, frameSize, 0x7b, 0xc8, 0xea, 0x30, 0x00, 0x64, 0x00};
        crsf2msp.reset();
        TEST_ASSERT_FALSE(crsf2mspExact(chunk));
    }

    // v2 new frame whose payload ends before the MSP v2 length field ([flags][func lo][func hi][len lo][len hi])
    for (uint8_t payloadLen = 0; payloadLen < 5; payloadLen++)
    {
        uint8_t chunk[16] = {0xc8, (uint8_t)(5 + payloadLen), 0x7b, 0xc8, 0xea, 0x50, 0x00, 0x64, 0x00, 0x00, 0x00};
        crsf2msp.reset();
        TEST_ASSERT_FALSE(crsf2mspExact(chunk));
    }

    // v1 jumbo new frame whose payload ends before the jumbo length field ([0xff][cmd][len lo][len hi])
    for (uint8_t payloadLen = 1; payloadLen < 4; payloadLen++)
    {
        uint8_t chunk[16] = {0xc8, (uint8_t)(5 + payloadLen), 0x7b, 0xc8, 0xea, 0x30, 0xff, 0x64, 0x00, 0x00};
        crsf2msp.reset();
        TEST_ASSERT_FALSE(crsf2mspExact(chunk));
    }

    // parser must still work afterwards
    crsf2msp.reset();
    runTest(MSP_IDENT, sizeof(MSP_IDENT));
}

// Unity setup/teardown
void setUp()
{

}
void tearDown() {}

int main(int argc, char **argv)
{
    UNITY_BEGIN();

    RUN_TEST(MSP_IDENT_TEST);
    RUN_TEST(MSPV2_HELLO_WORLD_TEST);
    RUN_TEST(MSPV2_IN_V1_HELLOWORLD_TEST);
    RUN_TEST(MSPV2_2_CHUNK_MSG);
    RUN_TEST(MSPV1_81_TEST);
    RUN_TEST(MSPV1_JUMBO_289_TEST);
    RUN_TEST(MSPV2_SERIAL_SETTINGS_TEST);
    RUN_TEST(MSPV2_OVERSIZED_REJECT_TEST);
    RUN_TEST(MSPV1_JUMBO_OVERSIZED_REJECT_TEST);
    RUN_TEST(CRSF_MSP_ERROR_REPLY_TEST);
    RUN_TEST(MSP2CRSF_TRUNCATED_REJECT_TEST);
    RUN_TEST(MSP2CRSF_SHORT_FRAME_REJECT_TEST);
    RUN_TEST(MSP2CRSF_LARGE_V2_FRAME_TEST);
    RUN_TEST(CRSF2MSP_SHORT_CHUNK_REJECT_TEST);

    UNITY_END();

    return 0;
}

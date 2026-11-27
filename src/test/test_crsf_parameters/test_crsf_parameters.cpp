#include <cstring>

#include "unity.h"

#include "CRSFRouter.h"
#include "crsf_protocol.h"

CRSFRouter crsfRouter;

// Connector standing in for the handset: records the frames delivered to it
class MockConnector final : public CRSFConnector
{
public:
    uint8_t maxPacketBytes = CRSF_MAX_PACKET_LEN;
    uint32_t forwardCount = 0;
    uint8_t lastMessage[CRSF_MAX_PACKET_LEN] {};

    MockConnector() { addDevice(CRSF_ADDRESS_RADIO_TRANSMITTER); }

    void forwardMessage(const crsf_header_t *message) override
    {
        forwardCount++;
        memcpy(lastMessage, message, std::min<size_t>(message->frame_size + CRSF_FRAME_NOT_COUNTED_BYTES, sizeof(lastMessage)));
    }

    uint8_t GetMaxPacketBytes() const override { return maxPacketBytes; }

    void reset()
    {
        forwardCount = 0;
        memset(lastMessage, 0, sizeof(lastMessage));
    }
};

class TestEndpoint final : public CRSFEndpoint
{
public:
    TestEndpoint() : CRSFEndpoint(CRSF_ADDRESS_CRSF_TRANSMITTER) {}
    void handleMessage(const crsf_header_t *message) override {}

    using CRSFEndpoint::parameterUpdateReq;
    using CRSFEndpoint::registerParameter;
};

static MockConnector connector;
static TestEndpoint *endpoint;

static bool callbackCalled;
static int32_t callbackArg;
static void recordCallback(propertiesCommon *, int32_t arg)
{
    callbackCalled = true;
    callbackArg = arg;
}

// Long option string so the parameter needs several chunks
static selectionParameter longSelection = {
    {"Long Selection", CRSF_TEXT_SELECTION},
    0,
    "Option zero;Option one;Option two;Option three;Option four;Option five;Option six;Option seven;Option eight",
    STR_EMPTYSPACE
};
static selectionParameter shortSelection = {
    {"Short Selection", CRSF_TEXT_SELECTION},
    0,
    "A;B;C",
    STR_EMPTYSPACE
};
static int8Parameter uint8Param = {
    {"Uint8", CRSF_UINT8},
    {{(uint8_t)5, (uint8_t)1, (uint8_t)10}},
    STR_EMPTYSPACE
};
static int8Parameter int8Param = {
    {"Int8", CRSF_INT8},
    {.s = {(int8_t)0, (int8_t)-5, (int8_t)5}},
    STR_EMPTYSPACE
};
// int16 values are stored big-endian, as they are sent on the wire
static int16Parameter uint16Param = {
    {"Uint16", CRSF_UINT16},
    {{htobe16(1000), htobe16(100), htobe16(2000)}},
    STR_EMPTYSPACE
};

static uint8_t longSelectionId;
static uint8_t shortSelectionId;
static uint8_t uint8Id;
static uint8_t int8Id;
static uint8_t uint16Id;

void setUp()
{
    endpoint = new TestEndpoint();
    endpoint->registerParameter(&longSelection, &recordCallback);
    endpoint->registerParameter(&shortSelection, &recordCallback);
    endpoint->registerParameter(&uint8Param, &recordCallback);
    endpoint->registerParameter(&int8Param, &recordCallback);
    endpoint->registerParameter(&uint16Param, &recordCallback);
    longSelectionId = longSelection.common.id;
    shortSelectionId = shortSelection.common.id;
    uint8Id = uint8Param.common.id;
    int8Id = int8Param.common.id;
    uint16Id = uint16Param.common.id;

    connector.reset();
    connector.maxPacketBytes = CRSF_MAX_PACKET_LEN;
    callbackCalled = false;
    callbackArg = -1;
}

void tearDown()
{
    delete endpoint;
    endpoint = nullptr;
}

static void readParameter(const uint8_t id, uint8_t chunk)
{
    endpoint->parameterUpdateReq(CRSF_ADDRESS_RADIO_TRANSMITTER, CRSF_FRAMETYPE_PARAMETER_READ, id, &chunk);
}

static void writeParameter(const uint8_t id, const uint8_t *value)
{
    // payload buffer is sized like a real CRSF frame so wider types can be read
    uint8_t payload[CRSF_MAX_PACKET_LEN] {};
    memcpy(payload, value, 4);
    endpoint->parameterUpdateReq(CRSF_ADDRESS_RADIO_TRANSMITTER, CRSF_FRAMETYPE_PARAMETER_WRITE, id, payload);
}

static void writeU8(const uint8_t id, const uint8_t value)
{
    const uint8_t v[4] = {value};
    writeParameter(id, v);
}

// --- PARAMETER_READ chunk handling (regression for #3825) ---

void test_read_chunks_remaining_counts_down()
{
    readParameter(longSelectionId, 0);
    TEST_ASSERT_EQUAL_UINT32(1, connector.forwardCount);
    const auto *msg = (crsf_ext_header_t *)connector.lastMessage;
    TEST_ASSERT_EQUAL_UINT8(CRSF_FRAMETYPE_PARAMETER_SETTINGS_ENTRY, msg->type);
    TEST_ASSERT_EQUAL_UINT8(longSelectionId, msg->payload[0]);
    const uint8_t chunksRemaining = msg->payload[1];
    TEST_ASSERT_TRUE_MESSAGE(chunksRemaining > 0, "test parameter should need more than one chunk");

    // the last chunk reports 0 remaining
    connector.reset();
    readParameter(longSelectionId, chunksRemaining);
    TEST_ASSERT_EQUAL_UINT32(1, connector.forwardCount);
    TEST_ASSERT_EQUAL_UINT8(0, ((crsf_ext_header_t *)connector.lastMessage)->payload[1]);
}

void test_read_chunk_past_end_is_ignored()
{
    readParameter(longSelectionId, 0);
    const uint8_t chunkCnt = ((crsf_ext_header_t *)connector.lastMessage)->payload[1] + 1;

    for (const uint16_t chunk : {(uint16_t)chunkCnt, (uint16_t)(chunkCnt + 1), (uint16_t)255})
    {
        connector.reset();
        readParameter(longSelectionId, chunk);
        TEST_ASSERT_EQUAL_UINT32_MESSAGE(0, connector.forwardCount, "chunk beyond the parameter was sent");
    }
}

void test_read_with_tiny_max_packet_size_is_ignored()
{
    // smaller than the CRSF ext header + crc + chunk header: no room for data
    connector.maxPacketBytes = 8;
    readParameter(shortSelectionId, 0);
    TEST_ASSERT_EQUAL_UINT32(0, connector.forwardCount);
}

// --- PARAMETER_WRITE range checks ---

void test_write_uint8_in_range()
{
    writeU8(uint8Id, 1);
    TEST_ASSERT_TRUE(callbackCalled);
    TEST_ASSERT_EQUAL_INT32(1, callbackArg);
    callbackCalled = false;
    writeU8(uint8Id, 10);
    TEST_ASSERT_TRUE(callbackCalled);
    TEST_ASSERT_EQUAL_INT32(10, callbackArg);
}

void test_write_uint8_out_of_range_is_rejected()
{
    for (const uint8_t v : {(uint8_t)0, (uint8_t)11, (uint8_t)255})
    {
        callbackCalled = false;
        writeU8(uint8Id, v);
        TEST_ASSERT_FALSE_MESSAGE(callbackCalled, "out of range uint8 value accepted");
    }
}

void test_write_int8_range()
{
    writeU8(int8Id, (uint8_t)-5);
    TEST_ASSERT_TRUE(callbackCalled);
    TEST_ASSERT_EQUAL_INT32(-5, callbackArg);

    for (const int8_t v : {(int8_t)-6, (int8_t)6, (int8_t)-128, (int8_t)127})
    {
        callbackCalled = false;
        writeU8(int8Id, (uint8_t)v);
        TEST_ASSERT_FALSE_MESSAGE(callbackCalled, "out of range int8 value accepted");
    }
}

void test_write_uint16_range()
{
    const uint8_t ok[4] = {0x07, 0xd0}; // 2000
    writeParameter(uint16Id, ok);
    TEST_ASSERT_TRUE(callbackCalled);
    TEST_ASSERT_EQUAL_INT32(2000, callbackArg);

    const uint8_t tooLow[4] = {0x00, 0x63};  // 99
    const uint8_t tooHigh[4] = {0x07, 0xd1}; // 2001
    for (const uint8_t *v : {tooLow, tooHigh})
    {
        callbackCalled = false;
        writeParameter(uint16Id, v);
        TEST_ASSERT_FALSE_MESSAGE(callbackCalled, "out of range uint16 value accepted");
    }
}

void test_write_selection_range()
{
    writeU8(shortSelectionId, 2); // "C"
    TEST_ASSERT_TRUE(callbackCalled);
    TEST_ASSERT_EQUAL_INT32(2, callbackArg);

    for (const uint8_t v : {(uint8_t)3, (uint8_t)255})
    {
        callbackCalled = false;
        writeU8(shortSelectionId, v);
        TEST_ASSERT_FALSE_MESSAGE(callbackCalled, "selection index beyond the options accepted");
    }
}

void test_write_hidden_uint8_still_range_checked()
{
    LUA_FIELD_HIDE(uint8Param);
    writeU8(uint8Id, 200);
    LUA_FIELD_SHOW(uint8Param);
    TEST_ASSERT_FALSE(callbackCalled);
}

int main(int argc, char **argv)
{
    crsfRouter.addConnector(&connector);

    UNITY_BEGIN();
    RUN_TEST(test_read_chunks_remaining_counts_down);
    RUN_TEST(test_read_chunk_past_end_is_ignored);
    RUN_TEST(test_read_with_tiny_max_packet_size_is_ignored);
    RUN_TEST(test_write_uint8_in_range);
    RUN_TEST(test_write_uint8_out_of_range_is_rejected);
    RUN_TEST(test_write_int8_range);
    RUN_TEST(test_write_uint16_range);
    RUN_TEST(test_write_selection_range);
    RUN_TEST(test_write_hidden_uint8_still_range_checked);
    UNITY_END();

    return 0;
}

#include <cstring>

#include "unity.h"

#include "CRSFRouter.h"
#include "crsf_protocol.h" // For CRSF types and constants

// --- Mock CRSFConnector ---
class MockConnector final : public CRSFConnector
{
public:
    crsf_addr_e forwardedToAddr = CRSF_ADDRESS_BROADCAST; // Address it claims to forward to
    bool forwardMessageCalled = false;
    crsf_ext_header_t lastForwardedMessage{};

    explicit MockConnector(crsf_addr_e forwardsTo = CRSF_ADDRESS_BROADCAST)
        : forwardedToAddr(forwardsTo)
    {
        addDevice(forwardsTo);
    }

    // Implement pure virtual or necessary methods for testing
    void forwardMessage(const crsf_header_t *message) override
    {
        forwardMessageCalled = true;
        if (message)
        {
            if (message->type < CRSF_FRAMETYPE_DEVICE_PING)
                memcpy(&lastForwardedMessage, message, sizeof(crsf_header_t));
            else
                memcpy(&lastForwardedMessage, message, sizeof(crsf_ext_header_t));
        }
    }

    void reset()
    {
        forwardMessageCalled = false;
        memset(&lastForwardedMessage, 0, sizeof(lastForwardedMessage));
    }
};

// --- Mock CRSFEndpoint Derived Class ---
class MockEndpoint final : public CRSFEndpoint
{
public:
    bool handleMessageResult = false; // Controllable result for handleMessage
    bool handleMessageCalled = false;
    crsf_ext_header_t lastHandledMessage{};

    // Call base constructor
    explicit MockEndpoint(crsf_addr_e device_id = CRSF_ADDRESS_FLIGHT_CONTROLLER)
        : CRSFEndpoint(device_id) {}

    // Implement the pure virtual function
    void handleMessage(const crsf_header_t *message) override
    {
        handleMessageCalled = true;
        if (message)
        {
            if (message->type < CRSF_FRAMETYPE_DEVICE_PING)
                memcpy(&lastHandledMessage, message, sizeof(crsf_header_t));
            else
                memcpy(&lastHandledMessage, message, sizeof(crsf_ext_header_t));
        }
    }

    void reset()
    {
        handleMessageCalled = false;
        handleMessageResult = false;
        memset(&lastHandledMessage, 0, sizeof(lastHandledMessage));
    }
};

// --- Test Globals ---
static CRSFRouter *router;
static MockEndpoint *testEndpoint = nullptr;
static MockConnector *connector1 = nullptr;
static MockConnector *connector2 = nullptr;
static MockConnector *connector3 = nullptr;

// --- Helper to create a message ---
// Creates an extended frame for simplicity in tests involving destination addresses
void create_test_message(crsf_ext_header_t *msg, crsf_frame_type_e type, crsf_addr_e dest, crsf_addr_e origin)
{
    memset(msg, 0, sizeof(crsf_ext_header_t)); // Start clean
    msg->device_addr = dest;
    msg->frame_size = 4; // Size includes type, ext dest, ext src
    msg->type = type;
    msg->dest_addr = dest;
    msg->orig_addr = origin;
}

void create_simple_test_message(crsf_header_t *msg, crsf_frame_type_e type, crsf_addr_e dest_device_addr)
{
    memset(msg, 0, sizeof(crsf_header_t)); // Start clean
    msg->device_addr = dest_device_addr;
    msg->frame_size = 2; // Size includes type
    msg->type = type;
}

// --- Unity Setup and Teardown ---
void setUp()
{
    router = new CRSFRouter();
    testEndpoint = new MockEndpoint(CRSF_ADDRESS_FLIGHT_CONTROLLER);
    connector1 = new MockConnector(CRSF_ADDRESS_RADIO_TRANSMITTER);   // Knows about forwarding messages for CRSF_ADDRESS_RADIO_TRANSMITTER
    connector2 = new MockConnector(CRSF_ADDRESS_CRSF_RECEIVER); // Knows about forwarding messages for CRSF_ADDRESS_CRSF_RECEIVER
    connector3 = new MockConnector();                    // Doesn't specifically forward any address

    router->addEndpoint(testEndpoint);
    router->addConnector(connector1);
    router->addConnector(connector2);
    router->addConnector(connector3);

    testEndpoint->reset();
    connector1->reset();
    connector2->reset();
    connector3->reset();
}

void tearDown()
{
    delete testEndpoint;
    delete connector1;
    delete connector2;
    delete connector3;
    testEndpoint = nullptr;
    connector1 = nullptr;
    connector2 = nullptr;
    connector3 = nullptr;
}

// --- Test Cases for CRSFEndpoint ---

void test_ProcessMessage_AddressedToEndpoint_Handled()
{
    crsf_ext_header_t msg;
    create_test_message(&msg, CRSF_FRAMETYPE_DEVICE_PING, CRSF_ADDRESS_FLIGHT_CONTROLLER, CRSF_ADDRESS_RADIO_TRANSMITTER);

    router->processMessage(nullptr, (crsf_header_t *)&msg);

    TEST_ASSERT_TRUE(testEndpoint->handleMessageCalled);
    TEST_ASSERT_EQUAL_UINT8(CRSF_ADDRESS_FLIGHT_CONTROLLER, testEndpoint->lastHandledMessage.dest_addr);
    TEST_ASSERT_FALSE(connector1->forwardMessageCalled);
    TEST_ASSERT_FALSE(connector2->forwardMessageCalled);
    TEST_ASSERT_FALSE(connector3->forwardMessageCalled);
}

void test_ProcessMessage_NotAddressedToEndpoint_NotHandled()
{
    crsf_ext_header_t msg;
    create_test_message(&msg, CRSF_FRAMETYPE_DEVICE_PING, CRSF_ADDRESS_CRSF_TRANSMITTER, CRSF_ADDRESS_RADIO_TRANSMITTER);

    router->processMessage(nullptr, (crsf_header_t *)&msg);

    TEST_ASSERT_FALSE(testEndpoint->handleMessageCalled);
    TEST_ASSERT_TRUE(connector1->forwardMessageCalled);
    TEST_ASSERT_EQUAL_UINT8(CRSF_ADDRESS_CRSF_TRANSMITTER, connector1->lastForwardedMessage.dest_addr);
    TEST_ASSERT_TRUE(connector2->forwardMessageCalled);
    TEST_ASSERT_EQUAL_UINT8(CRSF_ADDRESS_CRSF_TRANSMITTER, connector2->lastForwardedMessage.dest_addr);
    TEST_ASSERT_TRUE(connector3->forwardMessageCalled);
    TEST_ASSERT_EQUAL_UINT8(CRSF_ADDRESS_CRSF_TRANSMITTER, connector3->lastForwardedMessage.dest_addr);
}

void test_ProcessMessage_AddressedToOther_KnownConnector()
{
    crsf_ext_header_t msg;
    create_test_message(&msg, CRSF_FRAMETYPE_DEVICE_PING, CRSF_ADDRESS_RADIO_TRANSMITTER, CRSF_ADDRESS_CRSF_RECEIVER);

    router->processMessage(nullptr, (crsf_header_t *)&msg);

    TEST_ASSERT_FALSE(testEndpoint->handleMessageCalled); // Not addressed here
    TEST_ASSERT_TRUE(connector1->forwardMessageCalled);   // Should be forwarded to connector1
    TEST_ASSERT_EQUAL_UINT8(CRSF_ADDRESS_RADIO_TRANSMITTER, connector1->lastForwardedMessage.dest_addr);
    TEST_ASSERT_FALSE(connector2->forwardMessageCalled);
    TEST_ASSERT_FALSE(connector3->forwardMessageCalled);
}

void test_ProcessMessage_AddressedToOther_KnownConnector_FromConnector1()
{
    crsf_ext_header_t msg;
    // Message for CRSF_ADDRESS_CRSF_RECEIVER (handled by connector2), coming *from* connector1
    create_test_message(&msg, CRSF_FRAMETYPE_DEVICE_PING, CRSF_ADDRESS_CRSF_RECEIVER, CRSF_ADDRESS_RADIO_TRANSMITTER);

    router->processMessage(connector1, (crsf_header_t *)&msg); // Source connector is connector1

    TEST_ASSERT_FALSE(testEndpoint->handleMessageCalled);
    TEST_ASSERT_FALSE(connector1->forwardMessageCalled); // Not forwarded back to source
    TEST_ASSERT_TRUE(connector2->forwardMessageCalled);  // Should be forwarded to connector2
    TEST_ASSERT_EQUAL_UINT8(CRSF_ADDRESS_CRSF_RECEIVER, connector2->lastForwardedMessage.dest_addr);
    TEST_ASSERT_FALSE(connector3->forwardMessageCalled);
}

void test_ProcessMessage_AddressedToOther_UnknownConnector()
{
    crsf_ext_header_t msg;
    create_test_message(&msg, CRSF_FRAMETYPE_DEVICE_PING, CRSF_ADDRESS_ELRS_LUA, CRSF_ADDRESS_RADIO_TRANSMITTER);

    router->processMessage(nullptr, (crsf_header_t *)&msg);

    TEST_ASSERT_FALSE(testEndpoint->handleMessageCalled);
    TEST_ASSERT_TRUE(connector1->forwardMessageCalled); // Broadcast because destination unknown
    TEST_ASSERT_TRUE(connector2->forwardMessageCalled); // Broadcast
    TEST_ASSERT_TRUE(connector3->forwardMessageCalled); // Broadcast
    TEST_ASSERT_EQUAL_UINT8(CRSF_ADDRESS_ELRS_LUA, connector1->lastForwardedMessage.dest_addr);
    TEST_ASSERT_EQUAL_UINT8(CRSF_ADDRESS_ELRS_LUA, connector2->lastForwardedMessage.dest_addr);
    TEST_ASSERT_EQUAL_UINT8(CRSF_ADDRESS_ELRS_LUA, connector3->lastForwardedMessage.dest_addr);
}

void test_ProcessMessage_Broadcast_FromNull()
{
    crsf_ext_header_t msg;
    create_test_message(&msg, CRSF_FRAMETYPE_DEVICE_PING, CRSF_ADDRESS_BROADCAST, CRSF_ADDRESS_RADIO_TRANSMITTER);

    router->processMessage(nullptr, (crsf_header_t *)&msg);

    TEST_ASSERT_TRUE(testEndpoint->handleMessageCalled);
    TEST_ASSERT_TRUE(connector1->forwardMessageCalled);
    TEST_ASSERT_TRUE(connector2->forwardMessageCalled);
    TEST_ASSERT_TRUE(connector3->forwardMessageCalled);
    TEST_ASSERT_EQUAL_UINT8(CRSF_ADDRESS_BROADCAST, connector1->lastForwardedMessage.dest_addr);
}

void test_ProcessMessage_Broadcast_FromConnector2()
{
    crsf_ext_header_t msg;
    create_test_message(&msg, CRSF_FRAMETYPE_DEVICE_PING, CRSF_ADDRESS_BROADCAST, CRSF_ADDRESS_CRSF_RECEIVER);

    router->processMessage(connector2, (crsf_header_t *)&msg); // Source connector is connector2

    TEST_ASSERT_TRUE(testEndpoint->handleMessageCalled);
    TEST_ASSERT_TRUE(connector1->forwardMessageCalled);
    TEST_ASSERT_FALSE(connector2->forwardMessageCalled);
    TEST_ASSERT_TRUE(connector3->forwardMessageCalled);
}

void test_ProcessMessage_ExtendedFrameFromConnector_CallsAddDevice()
{
    crsf_ext_header_t msg;
    constexpr crsf_addr_e origin_addr = CRSF_ADDRESS_ELRS_LUA;
    create_test_message(&msg, CRSF_FRAMETYPE_DEVICE_PING, CRSF_ADDRESS_FLIGHT_CONTROLLER, origin_addr);

    TEST_ASSERT_FALSE(connector1->forwardsTo(origin_addr));

    router->processMessage(connector1, (crsf_header_t *)&msg);

    TEST_ASSERT_TRUE(connector1->forwardsTo(origin_addr));
    TEST_ASSERT_FALSE(connector2->forwardsTo(origin_addr));
    TEST_ASSERT_FALSE(connector3->forwardsTo(origin_addr));
}

void test_ProcessMessage_SimpleFrame_AddressedToOther_KnownConnector()
{
    crsf_header_t msg;
    create_simple_test_message(&msg, CRSF_FRAMETYPE_VARIO, CRSF_ADDRESS_RADIO_TRANSMITTER);

    router->processMessage(nullptr, &msg);

    TEST_ASSERT_TRUE(testEndpoint->handleMessageCalled);
    TEST_ASSERT_EQUAL_UINT8(CRSF_ADDRESS_RADIO_TRANSMITTER, testEndpoint->lastHandledMessage.device_addr);
    TEST_ASSERT_FALSE(testEndpoint->handleMessageResult);

    // Now check forwarding
    TEST_ASSERT_TRUE(connector1->forwardMessageCalled);
    // Check the forwarded message still looks like the original legacy message
    TEST_ASSERT_EQUAL_UINT8(CRSF_ADDRESS_RADIO_TRANSMITTER, connector1->lastForwardedMessage.device_addr);
    TEST_ASSERT_EQUAL_UINT8(CRSF_FRAMETYPE_VARIO, connector1->lastForwardedMessage.type);
    // Simple messages should be forwarded to all connectors
    TEST_ASSERT_TRUE(connector2->forwardMessageCalled);
    TEST_ASSERT_TRUE(connector3->forwardMessageCalled);
}

// --- Test Runner ---
int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_ProcessMessage_AddressedToEndpoint_Handled);
    RUN_TEST(test_ProcessMessage_NotAddressedToEndpoint_NotHandled);
    RUN_TEST(test_ProcessMessage_AddressedToOther_KnownConnector);
    RUN_TEST(test_ProcessMessage_AddressedToOther_KnownConnector_FromConnector1);
    RUN_TEST(test_ProcessMessage_AddressedToOther_UnknownConnector);
    RUN_TEST(test_ProcessMessage_Broadcast_FromNull);
    RUN_TEST(test_ProcessMessage_Broadcast_FromConnector2);
    RUN_TEST(test_ProcessMessage_ExtendedFrameFromConnector_CallsAddDevice);
    RUN_TEST(test_ProcessMessage_SimpleFrame_AddressedToOther_KnownConnector);
    return UNITY_END();
}

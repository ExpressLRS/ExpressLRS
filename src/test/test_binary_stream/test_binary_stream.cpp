#include <unity.h>
#include "common.h"
#include "../test_msp/mock_serial.h"

void setUp() {}
void tearDown() {}

void test_binary_stream_distinguishes_byte_values_from_eof()
{
    std::string bytes;
    for (unsigned value = 0; value < 256; ++value) bytes.push_back(static_cast<char>(value));
    StringStream stream(bytes);
    for (unsigned value = 0; value < 256; ++value)
    {
        TEST_ASSERT_EQUAL_INT(value, stream.peek());
        TEST_ASSERT_EQUAL_INT(value, stream.read());
    }
    TEST_ASSERT_EQUAL_INT(-1, stream.peek());
    TEST_ASSERT_EQUAL_INT(-1, stream.read());

    StringStream bulk(bytes);
    uint8_t result[256] = {};
    TEST_ASSERT_EQUAL(256, bulk.readBytes(result, sizeof(result)));
    TEST_ASSERT_EQUAL_UINT8_ARRAY(reinterpret_cast<const uint8_t *>(bytes.data()), result, sizeof(result));
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_binary_stream_distinguishes_byte_values_from_eof);
    return UNITY_END();
}

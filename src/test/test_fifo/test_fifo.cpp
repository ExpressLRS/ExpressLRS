#include <cstdint>
#include <FIFO.h>
#include <unity.h>
#include <set>

using namespace std;

const uint32_t fifoSize = 256;
FIFO<fifoSize> f;

void init()
{
    f.flush();
    for(int i=0;i<fifoSize/2;i++) {
        f.push(i);
        TEST_ASSERT_EQUAL(i, f.pop());
    }
    for(int i=0;i<fifoSize-1;i++) {
        f.push(i);
    }
}

void test_fifo_pop_wrap(void)
{
    init();
    for(int i=0;i<fifoSize-1;i++) {
        TEST_ASSERT_EQUAL(i, f.pop());
    }
}

void test_fifo_popBytes_wrap(void)
{
    init();
    uint8_t buf[fifoSize] = {0};
    f.popBytes(buf, fifoSize-1);
    for(int i=0;i<fifoSize-1;i++) {
        TEST_ASSERT_EQUAL(i, buf[i]);
    }
}

void test_fifo_ensure()
{
    f.flush();
    // Push 25 9-byte packets with a 1 byte length header
    for (int i = 0; i < 25; i++)
    {
        f.push(9); // 9 bytes;
        for(int x = 0 ; x<9 ; x++)
            f.push(i);
    }
    TEST_ASSERT_EQUAL(250, f.size());

    // Ensure we can push a 99 byte packet with a prefix (i.e. 100 bytes)
    f.ensure(100);
    TEST_ASSERT_EQUAL(150, f.size());   // make sure that we've popped the right amount to fit our jumbo packet
    TEST_ASSERT_EQUAL(9, f.pop());      // check that we have a header len
    for (int x = 0; x < 9; x++)
        TEST_ASSERT_EQUAL(10, f.pop()); // and that all the bytes in the head packet are what we expect
}

void test_fifo_skip_basic()
{
    f.flush();

    // Add 5 elements
    for(int i = 0; i < 5; i++) {
        f.push(i + 10);
    }
    TEST_ASSERT_EQUAL(5, f.size());

    // Skip 3 elements
    f.skip(3);
    TEST_ASSERT_EQUAL(2, f.size());

    // Remaining elements should be correct
    TEST_ASSERT_EQUAL(13, f.pop());
    TEST_ASSERT_EQUAL(14, f.pop());
    TEST_ASSERT_EQUAL(0, f.size());
}

void test_fifo_skip_overflow()
{
    f.flush();

    // Add 5 elements
    for(int i = 0; i < 5; i++) {
        f.push(i + 10);
    }
    TEST_ASSERT_EQUAL(5, f.size());

    // Try to skip 20 elements (but only 5 exist)
    // This should skip all 5 and leave FIFO empty
    f.skip(20);
    TEST_ASSERT_EQUAL(0, f.size());

    // FIFO should be in consistent state - can add new elements
    f.push(99);
    TEST_ASSERT_EQUAL(1, f.size());
    TEST_ASSERT_EQUAL(99, f.pop());
    TEST_ASSERT_EQUAL(0, f.size());
}

void test_fifo_skip_wraparound()
{
    f.flush();

    // Fill buffer near capacity and advance head by popping
    for(int i = 0; i < 200; i++) {
        f.push(i);
        f.pop();  // Advance head to near end of buffer
    }

    // Add elements that will wrap around
    f.push(100);
    f.push(101);
    f.push(102);
    TEST_ASSERT_EQUAL(3, f.size());

    // Skip more elements than exist with wrap-around
    f.skip(10);
    TEST_ASSERT_EQUAL(0, f.size());

    // Should be able to add elements normally
    f.push(200);
    f.push(201);
    TEST_ASSERT_EQUAL(2, f.size());
    TEST_ASSERT_EQUAL(200, f.pop());
    TEST_ASSERT_EQUAL(201, f.pop());
}

void test_fifo_skip_zero()
{
    f.flush();

    // Add elements
    f.push(10);
    f.push(11);
    TEST_ASSERT_EQUAL(2, f.size());

    // Skip zero elements should not change anything
    f.skip(0);
    TEST_ASSERT_EQUAL(2, f.size());
    TEST_ASSERT_EQUAL(10, f.pop());
    TEST_ASSERT_EQUAL(11, f.pop());
}

void test_fifo_skip_empty()
{
    f.flush();
    TEST_ASSERT_EQUAL(0, f.size());

    // Skip on empty FIFO should do nothing
    f.skip(10);
    TEST_ASSERT_EQUAL(0, f.size());

    // Should still be able to add elements
    f.push(42);
    TEST_ASSERT_EQUAL(1, f.size());
    TEST_ASSERT_EQUAL(42, f.pop());
}

// Unity setup/teardown
void setUp() {}
void tearDown() {}

int main(int argc, char **argv)
{
    UNITY_BEGIN();
    RUN_TEST(test_fifo_pop_wrap);
    RUN_TEST(test_fifo_popBytes_wrap);
    RUN_TEST(test_fifo_ensure);
    RUN_TEST(test_fifo_skip_basic);
    RUN_TEST(test_fifo_skip_overflow);
    RUN_TEST(test_fifo_skip_wraparound);
    RUN_TEST(test_fifo_skip_zero);
    RUN_TEST(test_fifo_skip_empty);
    UNITY_END();

    return 0;
}

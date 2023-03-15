#include <cstdint>
#include <FIFO.h>
#include <unity.h>
#include <set>

using namespace std;

FIFO f;
void init()
{
    f.flush();
    for(int i=0;i<FIFO::FIFO_SIZE/2;i++) {
        f.push(i);
        TEST_ASSERT_EQUAL(i, f.pop());
    }
    for(int i=0;i<FIFO::FIFO_SIZE-1;i++) {
        f.push(i);
    }
}

void test_fifo_pop_wrap(void)
{
    init();
    for(int i=0;i<FIFO::FIFO_SIZE-1;i++) {
        TEST_ASSERT_EQUAL(i, f.pop());
    }
}

void test_fifo_popBytes_wrap(void)
{
    init();
    uint8_t buf[FIFO::FIFO_SIZE] = {0};
    f.popBytes(buf, FIFO::FIFO_SIZE-1);
    for(int i=0;i<FIFO::FIFO_SIZE-1;i++) {
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

// Unity setup/teardown
void setUp() {}
void tearDown() {}

int main(int argc, char **argv)
{
    UNITY_BEGIN();
    RUN_TEST(test_fifo_pop_wrap);
    RUN_TEST(test_fifo_popBytes_wrap);
    RUN_TEST(test_fifo_ensure);
    UNITY_END();

    return 0;
}

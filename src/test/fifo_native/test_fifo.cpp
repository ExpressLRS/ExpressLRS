#include <cstdint>
#include <FIFO.h>
#include <unity.h>
#include <set>

using namespace std;

FIFO f;
void init()
{
    f.flush();
    for(int i=0;i<FIFO_SIZE/2;i++) {
        f.push(i);
        TEST_ASSERT_EQUAL(i, f.pop());
    }
    for(int i=0;i<FIFO_SIZE-1;i++) {
        f.push(i);
    }
}

void test_fifo_pop_wrap(void)
{
    init();
    for(int i=0;i<FIFO_SIZE-1;i++) {
        TEST_ASSERT_EQUAL(i, f.pop());
    }
}

void test_fifo_popBytes_wrap(void)
{
    init();
    uint8_t buf[FIFO_SIZE] = {0};
    f.popBytes(buf, FIFO_SIZE-1);
    for(int i=0;i<FIFO_SIZE-1;i++) {
        TEST_ASSERT_EQUAL(i, buf[i]);
    }
}

int main(int argc, char **argv)
{
    UNITY_BEGIN();
    RUN_TEST(test_fifo_pop_wrap);
    RUN_TEST(test_fifo_popBytes_wrap);
    UNITY_END();

    return 0;
}

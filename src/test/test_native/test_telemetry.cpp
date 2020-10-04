#include <telemetry.h>
#include <unity.h>

Telemetry telemetry;

void test_function_uart_in(void) {
    TEST_ASSERT_EQUAL(true, telemetry.RXhandleUARTin(0xEC));
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_function_uart_in);
    UNITY_END();

    return 0;
}
#include <Arduino.h>
#include <unity.h>
#include "mock_serial.h"
#include "msp_tests.h"
#include "test_switches.h"

void setup() {
    // NOTE!!! Wait for >2 secs
    // if board doesn't support software reset via Serial.DTR/RTS
    delay(2000);

    UNITY_BEGIN(); // IMPORTANT LINE!

    RUN_TEST(test_msp_receive);
    RUN_TEST(test_msp_send);

    setup_switches();
}

void loop() {
    UNITY_END(); // stop unit testing
}

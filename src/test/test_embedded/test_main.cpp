#define UNIT_TEST

#include <Arduino.h>
#include <unity.h>
#include "mock_serial.h"
#include "encapsulated_msp_tests.h"
#include "eeprom_tests.h"

void setup() {
    // NOTE!!! Wait for >2 secs
    // if board doesn't support software reset via Serial.DTR/RTS
    delay(2000);

    UNITY_BEGIN(); // IMPORTANT LINE!

    RUN_TEST(test_encapsulated_msp_send);
    RUN_TEST(test_eeprom_rw);
}

void loop() {
    UNITY_END(); // stop unit testing
}

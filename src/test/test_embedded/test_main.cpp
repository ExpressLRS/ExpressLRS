#define UNIT_TEST

#include <Arduino.h>
#include <unity.h>
#include "eeprom_tests.h"

// Unity setup/teardown
void setUp() {}
void tearDown() {}

void setup() {
    // NOTE!!! Wait for >2 secs
    // if board doesn't support software reset via Serial.DTR/RTS
    delay(2000);

    UNITY_BEGIN(); // IMPORTANT LINE!

    RUN_TEST(test_eeprom_rw);
}

void loop() {
    UNITY_END(); // stop unit testing
}

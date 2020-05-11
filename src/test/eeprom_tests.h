#include <Arduino.h>
#include <unity.h>
#include "elrs_eeprom.h"

ELRS_EEPROM eeprom;

void test_eeprom_rw(void)
{
    // TEST CASE:
    // GIVEN the eeprom object has been initialised
    // WHEN the write method is called using a valid address and value
    // THEN the value is stored at that address in eeprom

    // Initialise the eeprom
    eeprom.Begin();

    uint8_t address;
    uint8_t value;
    uint8_t success;
    
    // Write the first value variable into the eeprom at the specified address
    address = 1;
    value = 1;
    eeprom.WriteByte(address, value);

    // Assert that value in eeprom matches what was written
    TEST_ASSERT_EQUAL(value, eeprom.ReadByte(address));

    // Write the second value variable into the eeprom at the specified address
    address = 2;
    value = 2;
    eeprom.WriteByte(address, value);

    // Assert that value in eeprom matches what was written
    TEST_ASSERT_EQUAL(value, eeprom.ReadByte(address));
}

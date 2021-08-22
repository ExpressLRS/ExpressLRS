#pragma once

#include <stdint.h>

#define RESERVED_EEPROM_SIZE 1024

class ELRS_EEPROM
{
public:
    void Begin();
    uint8_t ReadByte(const uint32_t address);
    void WriteByte(const uint32_t address, const uint8_t value);
    void Commit();

    // The extEEPROM lib that we use for STM doesn't have the get and put templates
    // These templates need to be reimplemented here
    template <typename T> void Get(uint32_t addr, T &value)
    {
        uint8_t* p = (uint8_t*)(void*)&value;
        size_t   i = sizeof(value);
        while(i--)  *p++ = ReadByte(addr++);
    };

    template <typename T> const void Put(uint32_t addr, const T &value)
    {
        const uint8_t* p = (const uint8_t*)(const void*)&value;
        size_t         i = sizeof(value);
        while(i--)  WriteByte(addr++, *p++);
    };
};

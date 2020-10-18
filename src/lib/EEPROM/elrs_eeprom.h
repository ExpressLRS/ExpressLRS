#pragma once

#include <Arduino.h>
#include "../../src/targets.h"
#include <Wire.h>
#ifdef PLATFORM_STM32
    #include <extEEPROM.h>
#else
    #include <EEPROM.h>
#endif

#define RESERVED_EEPROM_SIZE 1024

class ELRS_EEPROM
{
public:
    void Begin();
    uint8_t ReadByte(const unsigned long address);
    void WriteByte(const unsigned long address, const uint8_t value);
    void Commit();
    
    // The extEEPROM lib that we use for STM doesn't have the get and put templates
    // These templates need to be reimplemented here
    template <typename T> void Get(int addr, T &value)
    {
        byte* p = (byte*)(void*)&value;
        byte  i = sizeof(value);
        while(i--)  *p++ = ReadByte(addr++);
    };

    template <typename T> const void Put(int addr, const T &value)
    {
        const byte* p = (const byte*)(const void*)&value;
        byte        i = sizeof(value);
        while(i--)  WriteByte(addr++, *p++);
    };
};

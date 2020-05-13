#pragma once

#include <Arduino.h>
#include "msp.h"

// Mock the serial port using a string stream class
// This will allow us to assert what gets sent on the serial port
class StringStream : public Stream
{
public:
    StringStream(String &s) : string(s), position(0) { }

    // Stream methods
    virtual int available() { return string.length() - position; }
    virtual int read() { return position < string.length() ? string[position++] : -1; }
    virtual int peek() { return position < string.length() ? string[position] : -1; }
    virtual void flush() { };
    // Print methods
    virtual size_t write(uint8_t c) { string += (char)c; return 1; };

private:
    String &string;
    unsigned int length;
    unsigned int position;
};

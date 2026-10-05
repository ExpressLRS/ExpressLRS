#pragma once

#include <string>
#include "msp.h"

// Mock the serial port using a string stream class
// This will allow us to assert what gets sent on the serial port
class StringStream : public Stream
{
public:
    StringStream(std::string &s) : buf(s), position(0) { }

    // Stream methods
    int available() { return buf.length() - position; }
    int read() { return position < buf.length() ? buf[position++] : -1; }
    int peek() { return position < buf.length() ? buf[position] : -1; }
    void flush() { }

    // Print methods
    size_t write(uint8_t c)
    { 
        buf += (char)c; return 1; 
    }
    size_t write(const uint8_t *c, size_t l)
    {
        for (int i=0; i<l ; buf += *c, i++, c++);
        return l;
    }

private:
    std::string &buf;
    unsigned int position;
};

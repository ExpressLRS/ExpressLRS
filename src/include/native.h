#pragma once

#include <stdint.h>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <cctype>

typedef uint8_t byte;

class Stream
{
public:
    // Stream methods
    int available() {return 0;}
    int read() {return -1;}
    int peek() {return 0;}
    void flush() {}
    // Print methods
    size_t write(uint8_t c) {return 1;}
    size_t write(uint8_t *s, int l) {return l;}
};

class HardwareSerial: public Stream {
public:
    int print(const char *s) {return 0;}
    int print(uint8_t s) {return 0;}
    int println(const char *s) {return 0;}
    int println(uint8_t s) {return 0;}
};

static HardwareSerial Serial;

inline void interrupts() {}
inline void noInterrupts() {}
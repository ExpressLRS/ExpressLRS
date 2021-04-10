#pragma once

#include <stdint.h>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <cctype>
#include <unistd.h>
#include <sys/time.h>

typedef uint8_t byte;

#define HEX 16

class Stream
{
public:
    Stream() {}
    virtual ~Stream() {}

    // Stream methods
    virtual int available() = 0;
    virtual int read() = 0;
    virtual int peek() = 0;
    virtual void flush() = 0;

    // Print methods
    virtual size_t write(uint8_t c) = 0;
    virtual size_t write(uint8_t *s, int l) = 0;
};

class HardwareSerial: public Stream {
public:
    // Stream methods
    int available() {return 0;}
    int read() {return -1;}
    int peek() {return 0;}
    void flush() {}

    // Print methods
    size_t write(uint8_t c) {return 1;}
    size_t write(uint8_t *s, int l) {return l;}

    int print(const char *s) {return 0;}
    int print(uint8_t s) {return 0;}
    int print(uint8_t s, int radix) {return 0;}
    int println() {return 0;}
    int println(const char *s) {return 0;}
    int println(uint8_t s) {return 0;}
    int println(uint8_t s, int radix) {return 0;}
};

static HardwareSerial Serial;

inline void interrupts() {}
inline void noInterrupts() {}

inline unsigned long micros() {
    struct timeval tv;
    gettimeofday(&tv,NULL);
    return tv.tv_sec*(uint64_t)1000000+tv.tv_usec;
}

inline void delay(int32_t time) {
    usleep(time);
}

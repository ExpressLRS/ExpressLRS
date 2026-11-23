// Minimal Arduino API for compiling the ELRS screen code on a PC (screensim)
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <string>
#include <algorithm>

#define PROGMEM
#define PGM_P const char *
#define pgm_read_byte(a) (*(const uint8_t *)(a))
#define pgm_read_word(a) (*(const uint16_t *)(a))
#define pgm_read_dword(a) (*(const uint32_t *)(a))
#define pgm_read_pointer(a) (*(void *const *)(a))
#define pgm_read_ptr(a) (*(void *const *)(a))
class __FlashStringHelper;
#define F(s) (reinterpret_cast<const __FlashStringHelper *>(s))
#define ICACHE_RAM_ATTR
#define IRAM_ATTR
#define HIGH 1
#define LOW 0
#define OUTPUT 1
#define INPUT 0
#define INPUT_PULLUP 2
#define LSBFIRST 0
#define MSBFIRST 1
#define DEC 10
#define HEX 16
#define bit(b) (1UL << (b))
#ifndef PI
#define PI 3.14159265358979323846
#endif
typedef bool boolean;
typedef uint8_t byte;

using std::min;
using std::max;
template <class T> static inline T constrain(T x, T a, T b) { return x < a ? a : (x > b ? b : x); }

uint32_t millis();
uint32_t micros();
static inline void delay(uint32_t) {}
static inline void delayMicroseconds(uint32_t) {}
static inline void yield() {}
static inline void pinMode(int, int) {}
static inline void digitalWrite(int, int) {}
static inline int digitalRead(int) { return 0; }
static inline void shiftOut(int, int, int, uint8_t) {}

class String
{
public:
    std::string s;
    String() {}
    String(const char *c) : s(c ? c : "") {}
    String(const std::string &x) : s(x) {}
    String(char c) : s(1, c) {}
    String(int v) : s(std::to_string(v)) {}
    String(unsigned v) : s(std::to_string(v)) {}
    String(long v) : s(std::to_string(v)) {}
    String(unsigned long v) : s(std::to_string(v)) {}
    const char *c_str() const { return s.c_str(); }
    unsigned length() const { return s.size(); }
    char operator[](unsigned i) const { return s[i]; }
    char charAt(unsigned i) const { return s[i]; }
    String &operator+=(const String &o) { s += o.s; return *this; }
    String &operator+=(const char *o) { s += o; return *this; }
    String &operator+=(char o) { s += o; return *this; }
    friend String operator+(const String &a, const String &b) { return String(a.s + b.s); }
    friend String operator+(const String &a, const char *b) { return String(a.s + b); }
    bool operator==(const char *o) const { return s == o; }
    void replace(const char *f, const char *r)
    {
        size_t p = 0, fl = strlen(f), rl = strlen(r);
        while ((p = s.find(f, p)) != std::string::npos) { s.replace(p, fl, r); p += rl; }
    }
};

class Print
{
public:
    virtual ~Print() {}
    virtual size_t write(uint8_t c) = 0;
    virtual size_t write(const uint8_t *b, size_t n) { size_t r = 0; while (n--) r += write(*b++); return r; }
    size_t write(const char *s) { return write((const uint8_t *)s, strlen(s)); }
    size_t write(const char *s, size_t n) { return write((const uint8_t *)s, n); }
    virtual void flush() {}
    virtual int availableForWrite() { return 4096; }
    size_t print(const char *s) { return write(s); }
    size_t print(const String &s) { return write(s.c_str()); }
    size_t print(char c) { return write((uint8_t)c); }
    size_t print(long v, int base = DEC) { char b[34]; snprintf(b, sizeof b, base == 16 ? "%lx" : "%ld", v); return write(b); }
    size_t print(int v, int base = DEC) { return print((long)v, base); }
    size_t print(unsigned v, int base = DEC) { return print((long)v, base); }
    size_t print(unsigned long v, int base = DEC) { return print((long)v, base); }
    size_t print(unsigned char v, int base = DEC) { return print((long)v, base); }
    size_t print(double v, int d = 2) { char b[34]; snprintf(b, sizeof b, "%.*f", d, v); return write(b); }
    size_t println() { return write("\r\n"); }
    template <class T> size_t println(T v) { size_t n = print(v); return n + println(); }
};

class Stream : public Print
{
public:
    virtual int available() = 0;
    virtual int read() = 0;
    virtual int peek() = 0;
};

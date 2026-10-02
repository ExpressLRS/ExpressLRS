// The Arduino/ESP names the firmware needs beyond what include/native.h provides. Force-included.
#pragma once

#include <string>
#include "native.h"

typedef std::string String;
typedef bool boolean;

#define OUTPUT 1
#define INPUT 0
#define INPUT_PULLUP 2
#define LOW 0
#define HIGH 1

inline void pinMode(int, int) {}
inline void digitalWrite(int, int) {}

inline int digitalRead(int)
{
    return 0;
}

inline void dacWrite(int, int) {}
inline void analogWrite(int, int) {}
inline void yield() {}

// Arduino's map(): rescales x from the range in_min..in_max to out_min..out_max
inline long map(long x, long in_min, long in_max, long out_min, long out_max)
{
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

struct FuzzFS
{
    bool begin()
    {
        return true;
    }

    void end() {}
    bool format()
    {
        return true;
    }
};
extern FuzzFS LittleFS;

struct FuzzESP
{
    void restart() {}
};
extern FuzzESP ESP;

struct FuzzWire
{
    void begin(int, int) {}
    void setClock(int) {}
};
extern FuzzWire Wire;

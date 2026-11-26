#pragma once
#include "Arduino.h"
class TwoWire { public: void begin(int=-1,int=-1) {} void setClock(uint32_t) {} void beginTransmission(uint8_t) {} uint8_t endTransmission(bool=true) { return 0; } size_t write(uint8_t) { return 1; } size_t write(const uint8_t *, size_t n) { return n; } };
extern TwoWire Wire;

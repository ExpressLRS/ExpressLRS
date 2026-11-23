#pragma once
#include "Arduino.h"
#define SPI_MODE0 0
#define SPI_MODE1 1
#define SPI_MODE2 2
#define SPI_MODE3 3
class SPISettings { public: SPISettings(uint32_t=0, uint8_t=0, uint8_t=0) {} };
class SPIClass { public: void begin(int=-1,int=-1,int=-1,int=-1) {} void end() {} void beginTransaction(SPISettings) {} void endTransaction() {}
  uint8_t transfer(uint8_t) { return 0; } void setClockDivider(int) {} void setDataMode(int) {} void setBitOrder(int) {} };
extern SPIClass SPI;

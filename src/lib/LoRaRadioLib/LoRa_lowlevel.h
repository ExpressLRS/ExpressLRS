#ifndef LoRa_lowlevel
#define LoRa_lowlevel

#include "LoRa_SX127x.h"
//#include "SPIdriver.h"


#ifndef ICACHE_RAM_ATTR //fix to allow both esp32 and esp8266
#define ICACHE_RAM_ATTR IRAM_ATTR
#endif

void initPins();

uint8_t readRegister(uint8_t reg);
uint8_t getRegValue(uint8_t reg, uint8_t msb = 7, uint8_t lsb = 0);

uint8_t setRegValue(uint8_t reg, uint8_t value, uint8_t msb = 7, uint8_t lsb = 0);

uint8_t readRegisterBurst(uint8_t reg, uint8_t numBytes, uint8_t* inBytes);

void writeRegister(uint8_t reg, uint8_t data);
void writeRegisterFIFO(uint8_t reg, const volatile uint8_t * data, uint8_t length);
void writeRegisterBurst(uint8_t reg, uint8_t * data, uint8_t numBytes);

#endif


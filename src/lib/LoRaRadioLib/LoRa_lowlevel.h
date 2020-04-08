#ifndef LoRa_lowlevel
#define LoRa_lowlevel

#define DebugVerbosity 1

#include <Arduino.h>

void initModule();

uint8_t ICACHE_RAM_ATTR getRegValue(uint8_t reg, uint8_t msb = 7, uint8_t lsb = 0);

uint8_t ICACHE_RAM_ATTR readRegisterBurst(uint8_t reg, uint8_t numBytes, uint8_t *inBytes);
//uint8_t ICACHE_RAM_ATTR readRegisterBurst(uint8_t reg, volatile uint8_t numBytes, volatile uint8_t *inBytes);
//uint8_t ICACHE_RAM_ATTR readRegisterBurst(uint8_t reg, uint8_t numBytes, char *str);

uint8_t ICACHE_RAM_ATTR readRegister(uint8_t reg);
uint8_t ICACHE_RAM_ATTR setRegValue(uint8_t reg, uint8_t value, uint8_t msb = 7, uint8_t lsb = 0);

void ICACHE_RAM_ATTR writeRegisterBurstStr(uint8_t reg, uint8_t *data, uint8_t numBytes);
//void ICACHE_RAM_ATTR writeRegisterBurstStr(uint8_t reg, const volatile uint8_t *data, uint8_t numBytes);
void ICACHE_RAM_ATTR writeRegister(uint8_t reg, uint8_t data);

#endif

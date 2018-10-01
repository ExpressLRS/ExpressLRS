#ifndef _LORALIB_MODULE_H
#define _LORALIB_MODULE_H

#include <SPI.h>

#include "TypeDef.h"


#define nss 18
#define dio0 26
#define dio1 25

#define SPI_READ  0b00000000
#define SPI_WRITE 0b10000000

//void initModule(int nss, int dio0, int dio1);
void initModule();

uint8_t getRegValue(uint8_t reg, uint8_t msb = 7, uint8_t lsb = 0);
uint8_t readRegisterBurst(uint8_t reg, uint8_t numBytes, uint8_t* inBytes);
uint8_t readRegisterBurstStr(uint8_t reg, uint8_t numBytes, char* str);
uint8_t readRegister(uint8_t reg);

uint8_t setRegValue(uint8_t reg, uint8_t value, uint8_t msb = 7, uint8_t lsb = 0);
void writeRegisterBurst(uint8_t reg, uint8_t* data, uint8_t numBytes);
void writeRegisterBurstStr(uint8_t reg, const char* data, uint8_t numBytes);
void writeRegister(uint8_t reg, uint8_t data);

int _nss;
int _dio0;
int _dio1;


#endif


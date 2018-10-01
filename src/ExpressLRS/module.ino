#include "Module.h"



void initModule() {
  _nss = nss;
  _dio0 = dio0;
  _dio1 = dio1;
  
  pinMode(_nss, OUTPUT);
  pinMode(_dio0, INPUT);
  pinMode(_dio1, INPUT);
  
  digitalWrite(_nss, HIGH);
  
  SPI.begin();
}

uint8_t getRegValue(uint8_t reg, uint8_t msb, uint8_t lsb) {
  if((msb > 7) || (lsb > 7) || (lsb > msb)) {
    return(ERR_INVALID_BIT_RANGE);
  }
  
  uint8_t rawValue = readRegister(reg);
  uint8_t maskedValue = rawValue & ((0b11111111 << lsb) & (0b11111111 >> (7 - msb)));
  return(maskedValue);
}

uint8_t readRegisterBurst(uint8_t reg, uint8_t numBytes, uint8_t* inBytes) {
  digitalWrite(_nss, LOW);
  SPI.transfer(reg | SPI_READ);
  for(uint8_t i = 0; i < numBytes; i++) {
    inBytes[i] = SPI.transfer(reg);
  }
  digitalWrite(_nss, HIGH);
  return(ERR_NONE);
}

uint8_t readRegisterBurstStr(uint8_t reg, uint8_t numBytes, char* inBytes) {
  digitalWrite(_nss, LOW);
  SPI.transfer(reg | SPI_READ);
  for(uint8_t i = 0; i < numBytes; i++) {
    inBytes[i] = SPI.transfer(reg);
  //Serial.println(inBytes[i]);
  }
  digitalWrite(_nss, HIGH);
  return(ERR_NONE);
}

uint8_t readRegister(uint8_t reg) {
  uint8_t inByte;
  digitalWrite(_nss, LOW);
  SPI.beginTransaction(SPISettings(2000000, MSBFIRST, SPI_MODE0));
  SPI.transfer(reg | SPI_READ);
  SPI.endTransaction();
  inByte = SPI.transfer(0x00);
  digitalWrite(_nss, HIGH);
  return(inByte);
}

uint8_t setRegValue(uint8_t reg, uint8_t value, uint8_t msb, uint8_t lsb) {
  if((msb > 7) || (lsb > 7) || (lsb > msb)) {
    return(ERR_INVALID_BIT_RANGE);
  }
  
  uint8_t currentValue = readRegister(reg);
  uint8_t mask = ~((0b11111111 << (msb + 1)) | (0b11111111 >> (8 - lsb)));
  uint8_t newValue = (currentValue & ~mask) | (value & mask);
  writeRegister(reg, newValue);
  return(ERR_NONE);
}

void writeRegisterBurst(uint8_t reg, uint8_t* data, uint8_t numBytes) {
  digitalWrite(_nss, LOW);
  SPI.transfer(reg | SPI_WRITE);
  for(uint8_t i = 0; i < numBytes; i++) {
    SPI.transfer(data[i]);
  }
  digitalWrite(_nss, HIGH);
}

void writeRegisterBurstStr(uint8_t reg, const char* data, uint8_t numBytes) {
  digitalWrite(_nss, LOW);
  SPI.transfer(reg | SPI_WRITE);
  for(uint8_t i = 0; i < numBytes; i++) {
    SPI.transfer(data[i]);
  }
  digitalWrite(_nss, HIGH);
}

void writeRegister(uint8_t reg, uint8_t data) {
  digitalWrite(_nss, LOW);
  SPI.beginTransaction(SPISettings(2000000, MSBFIRST, SPI_MODE0));
  SPI.transfer(reg | SPI_WRITE);
  SPI.transfer(data);
  SPI.endTransaction();
  digitalWrite(_nss, HIGH);
}


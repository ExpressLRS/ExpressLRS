#ifndef LoRa_lowlevel
#define LoRa_lowlevel

#include "LoRa_SX127x.h"
#include "SPIdriver.h"

void initModule(uint8_t nss, uint8_t dio0, uint8_t dio1);
void initModule();
void RFmoduleBegin(); 

uint8_t readRegister(uint8_t reg);
uint8_t getRegValue(uint8_t reg, uint8_t msb = 7, uint8_t lsb = 0);

uint8_t setRegValue(uint8_t reg, uint8_t value, uint8_t msb = 7, uint8_t lsb = 0);

uint8_t readRegisterBurst(uint8_t reg, const volatile uint8_t numBytes, const volatile uint8_t * inBytes);

uint8_t readRegisterBurst(uint8_t reg, volatile uint8_t numBytes, volatile uint8_t * inBytes);

uint8_t readRegisterBurst(uint8_t reg, uint8_t numBytes, uint8_t* inBytes);
uint8_t readRegisterBurst(uint8_t reg, uint8_t numBytes, char* str);

void writeRegisterFIFO(uint8_t reg, const volatile uint8_t * data, uint8_t length);

void writeRegisterBurst(uint8_t reg, uint8_t * data, uint8_t numBytes);
void writeRegisterBurst(uint8_t reg, const volatile uint8_t * data, const volatile uint8_t numBytes);

void writeRegisterBurstStr(uint8_t reg, uint8_t * data, uint8_t numBytes);
void writeRegisterBurstStr(uint8_t reg, const volatile uint8_t * data, uint8_t numBytes);

void writeRegister(uint8_t reg, uint8_t data);
void writeRegisterNR(uint8_t reg, uint8_t data);

typedef enum {DEBUG_1, DEBUG_2, DEBUG_3, DEBUG_4} Verbosity_; 


// uint8_t _nss;
// uint8_t _dio0;
// uint8_t _dio1;


#endif


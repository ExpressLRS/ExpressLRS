
#include "RadioAPI_lowlevel.h"
#include "SPIdriver.h"


iSPI iSPI;  //custom SPI lib with IRAM_ATTR on critical functions


extern Verbosity_ DebugVerbosity;

void initModule() {

  _nss = nss;
  _dio0 = dio0;
  _dio1 = dio1;

  pinMode(_nss, OUTPUT);
  pinMode(_dio0, INPUT);
  pinMode(_dio1, INPUT);

  digitalWrite(nss, HIGH);
  iSPI.setHost(VSPI_HOST);
  iSPI.init();
  //SPI.begin();
  //SPI.beginTransaction(SPISettings(9000000, MSBFIRST, SPI_MODE0));

  //SPI.usingInterrupt(255);
}


uint8_t IRAM_ATTR getRegValue(uint8_t reg, uint8_t msb, uint8_t lsb) {
  if ((msb > 7) || (lsb > 7) || (lsb > msb)) {
    return (ERR_INVALID_BIT_RANGE);
  }
  uint8_t rawValue = readRegister(reg);
  uint8_t maskedValue = rawValue & ((0b11111111 << lsb) & (0b11111111 >> (7 - msb)));
  return (maskedValue);
}



uint8_t IRAM_ATTR readRegisterBurst(uint8_t reg, uint8_t numBytes, uint8_t* inBytes) {
  char OutByte;
  digitalWrite(_nss, LOW);
  ////iSPI.beginTransaction(SPISettings(9000000, MSBFIRST, SPI_MODE0));
  OutByte = (reg | SPI_READ);
  iSPI.transferByte(OutByte);
  for (uint8_t i = 0; i < numBytes; i++) {
    inBytes[i] = iSPI.transferByte(reg);
  }
  ////////iSPI.endTransaction();
  digitalWrite(_nss, HIGH);

  if (DebugVerbosity >= DEBUG_4) {
    Serial.print("SPI: Read Burst ");
    Serial.print("REG: ");
    Serial.print(reg);
    Serial.print(" LEN: ");
    Serial.print(numBytes);
    Serial.print(" DATA: ");

    for (int i = 0; i < numBytes; i++) {
      Serial.print(inBytes[i]);
    }

    Serial.println();
  }

  return (ERR_NONE);
}

uint8_t IRAM_ATTR readRegisterBurstStr(uint8_t reg, uint8_t numBytes, char* inBytes) {
  digitalWrite(_nss, LOW);
  //iSPI.beginTransaction(SPISettings(9000000, MSBFIRST, SPI_MODE0));
  iSPI.transferByte(reg | SPI_READ);
  for (uint8_t i = 0; i < numBytes; i++) {
    inBytes[i] = iSPI.transferByte(reg);
    //Serial.println(inBytes[i]);
  }
  //////iSPI.endTransaction();
  digitalWrite(_nss, HIGH);

  if (DebugVerbosity >= DEBUG_4) {
    Serial.print("SPI: Read BurstStr ");
    Serial.print("REG: ");
    Serial.print(reg);
    Serial.print(" LEN: ");
    Serial.print(numBytes);
    Serial.print(" DATA: ");

    for (int i = 0; i < numBytes; i++) {
      Serial.print(inBytes[i]);
    }

    Serial.println();
  }

  return (ERR_NONE);
}


uint8_t IRAM_ATTR readRegister(uint8_t reg) {
  uint8_t InByte;
  uint8_t OutByte;
  digitalWrite(_nss, LOW);
  // iSPI.beginTransaction(SPISettings(9000000, MSBFIRST, SPI_MODE0));
  OutByte = (reg | SPI_READ);
  //Serial.println(OutByte,HEX);

  iSPI.transferByte(OutByte);

  //Serial.println(OutByte,HEX);

  InByte = iSPI.transferByte(0x00);
  //iSPI.sendByte(&OutByte, &InByte, (uint8_t)1);

  //  iSPI.endTransaction();
  digitalWrite(_nss, HIGH);

  //  if (1 == 1) {
  //    Serial.print("SPI: Read ");
  //    Serial.print("REG: ");
  //    Serial.print(reg);
  //    Serial.print(" VAL: ");
  //    Serial.println(InByte, HEX);
  //  }
  return (InByte);
}

uint8_t IRAM_ATTR setRegValue(uint8_t reg, uint8_t value, uint8_t msb, uint8_t lsb) {
  if ((msb > 7) || (lsb > 7) || (lsb > msb)) {
    return (ERR_INVALID_BIT_RANGE);
  }

  uint8_t currentValue = readRegister(reg);
  uint8_t mask = ~((0b11111111 << (msb + 1)) | (0b11111111 >> (8 - lsb)));
  uint8_t newValue = (currentValue & ~mask) | (value & mask);
  writeRegister(reg, newValue);
  return (ERR_NONE);


}

void IRAM_ATTR writeRegisterBurst(uint8_t reg, uint8_t* data, uint8_t numBytes) {
  digitalWrite(_nss, LOW);
  ////iSPI.beginTransaction(SPISettings(9000000, MSBFIRST, SPI_MODE0));
  iSPI.transferByte(reg | SPI_WRITE);
  //SPDR = (reg | SPI_WRITE);
  for (uint8_t i = 0; i < numBytes; i++) {
    iSPI.transferByte(data[i]);
  }
  //////iSPI.endTransaction();
  digitalWrite(_nss, HIGH);



}

void IRAM_ATTR writeRegisterBurstStr(uint8_t reg, const char* data, uint8_t numBytes) {
  digitalWrite(_nss, LOW);
  ////iSPI.beginTransaction(SPISettings(9000000, MSBFIRST, SPI_MODE0));
  iSPI.transferByte(reg | SPI_WRITE);
  for (uint8_t i = 0; i < numBytes; i++) {
    iSPI.transferByte(data[i]);
  }
  //////iSPI.endTransaction();
  digitalWrite(_nss, HIGH);
}

void IRAM_ATTR writeRegister(uint8_t reg, uint8_t data) {
  digitalWrite(_nss, LOW);
  //  iSPI.beginTransaction(SPISettings(9000000, MSBFIRST, SPI_MODE0));
  iSPI.transferByte(reg | SPI_WRITE);
  iSPI.transferByte(data);
  //  iSPI.endTransaction();
  digitalWrite(_nss, HIGH);

  if (DebugVerbosity >= DEBUG_4) {
    Serial.print("SPI: Write ");
    Serial.print("REG: ");
    Serial.print(reg);
    Serial.print(" VAL: ");
    Serial.println(data, HEX);
  }

}





//uint8_t IRAM_ATTR readRegisterBurst(uint8_t reg, uint8_t numBytes, uint8_t* inBytes) {
//
//  digitalWrite(_nss, LOW);
//  ////SPI.beginTransaction(SPISettings(9000000, MSBFIRST, SPI_MODE0));
//  SPI.transfer(reg | SPI_READ);
//  for (uint8_t i = 0; i < numBytes; i++) {
//    inBytes[i] = SPI.transfer(reg);
//  }
//  ////////SPI.endTransaction();
//  digitalWrite(_nss, HIGH);
//
//  if (DebugVerbosity >= DEBUG_4) {
//    Serial.print("SPI: Read Burst ");
//    Serial.print("REG: ");
//    Serial.print(reg);
//    Serial.print(" LEN: ");
//    Serial.print(numBytes);
//    Serial.print(" DATA: ");
//
//    for (int i = 0; i < numBytes; i++) {
//      Serial.print(inBytes[i]);
//    }
//
//    Serial.println();
//  }
//
//  return (ERR_NONE);
//}
//
//uint8_t IRAM_ATTR readRegisterBurstStr(uint8_t reg, uint8_t numBytes, char* inBytes) {
//  digitalWrite(_nss, LOW);
//  //SPI.beginTransaction(SPISettings(9000000, MSBFIRST, SPI_MODE0));
//  SPI.transfer(reg | SPI_READ);
//  for (uint8_t i = 0; i < numBytes; i++) {
//    inBytes[i] = SPI.transfer(reg);
//    //Serial.println(inBytes[i]);
//  }
//  //////SPI.endTransaction();
//  digitalWrite(_nss, HIGH);
//
//  if (DebugVerbosity >= DEBUG_4) {
//    Serial.print("SPI: Read BurstStr ");
//    Serial.print("REG: ");
//    Serial.print(reg);
//    Serial.print(" LEN: ");
//    Serial.print(numBytes);
//    Serial.print(" DATA: ");
//
//    for (int i = 0; i < numBytes; i++) {
//      Serial.print(inBytes[i]);
//    }
//
//    Serial.println();
//  }
//
//  return (ERR_NONE);
//}
//
//uint8_t IRAM_ATTR readRegister(uint8_t reg) {
//  uint8_t inByte;
//  digitalWrite(_nss, LOW);
//  SPI.beginTransaction(SPISettings(9000000, MSBFIRST, SPI_MODE0));
//  SPI.transfer(reg | SPI_READ);
//  inByte = SPI.transfer(0x00);
//  SPI.endTransaction();
//  digitalWrite(_nss, HIGH);
//
//  if (DebugVerbosity >= DEBUG_4) {
//    Serial.print("SPI: Read ");
//    Serial.print("REG: ");
//    Serial.print(reg);
//    Serial.print(" VAL: ");
//    Serial.println(inByte, HEX);
//  }
//  return (inByte);
//}
//
//uint8_t IRAM_ATTR setRegValue(uint8_t reg, uint8_t value, uint8_t msb, uint8_t lsb) {
//  if ((msb > 7) || (lsb > 7) || (lsb > msb)) {
//    return (ERR_INVALID_BIT_RANGE);
//  }
//
//  uint8_t currentValue = readRegister(reg);
//  uint8_t mask = ~((0b11111111 << (msb + 1)) | (0b11111111 >> (8 - lsb)));
//  uint8_t newValue = (currentValue & ~mask) | (value & mask);
//  writeRegister(reg, newValue);
//  return (ERR_NONE);
//
//
//}
//
//void IRAM_ATTR writeRegisterBurst(uint8_t reg, uint8_t* data, uint8_t numBytes) {
//  digitalWrite(_nss, LOW);
//  ////SPI.beginTransaction(SPISettings(9000000, MSBFIRST, SPI_MODE0));
//  SPI.transfer(reg | SPI_WRITE);
//  //SPDR = (reg | SPI_WRITE);
//  for (uint8_t i = 0; i < numBytes; i++) {
//    SPI.transfer(data[i]);
//  }
//  //////SPI.endTransaction();
//  digitalWrite(_nss, HIGH);
//
//
//
//}
//
//void IRAM_ATTR writeRegisterBurstStr(uint8_t reg, const char* data, uint8_t numBytes) {
//  digitalWrite(_nss, LOW);
//  ////SPI.beginTransaction(SPISettings(9000000, MSBFIRST, SPI_MODE0));
//  SPI.transfer(reg | SPI_WRITE);
//  for (uint8_t i = 0; i < numBytes; i++) {
//    SPI.transfer(data[i]);
//  }
//  //////SPI.endTransaction();
//  digitalWrite(_nss, HIGH);
//}
//
//void IRAM_ATTR writeRegister(uint8_t reg, uint8_t data) {
//  digitalWrite(_nss, LOW);
//  SPI.beginTransaction(SPISettings(9000000, MSBFIRST, SPI_MODE0));
//  SPI.transfer(reg | SPI_WRITE);
//  SPI.transfer(data);
//  SPI.endTransaction();
//  digitalWrite(_nss, HIGH);
//
//  if (DebugVerbosity >= DEBUG_4) {
//    Serial.print("SPI: Write ");
//    Serial.print("REG: ");
//    Serial.print(reg);
//    Serial.print(" VAL: ");
//    Serial.println(data, HEX);
//  }
//
//}


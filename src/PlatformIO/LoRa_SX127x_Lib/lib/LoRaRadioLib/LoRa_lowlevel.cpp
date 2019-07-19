
#include "LoRa_lowlevel.h"
//#include "SPIdriver.h"

#ifdef PLATFORM_ESP32
//iSPI iSPI; //custom SPI lib with IRAM_ATTR on critical functions

#include <SPI.h>
#else
#include <SPI.h>
#include <Arduino.h>
#endif

Verbosity_ DebugVerbosity = DEBUG_1;

void initModule(uint8_t nss, uint8_t dio0, uint8_t dio1)
{

#ifdef PLATFORM_ESP32
  pinMode(SX127xDriver::SX127x_nss, OUTPUT);
  pinMode(SX127xDriver::SX127x_dio0, INPUT);
  pinMode(SX127xDriver::SX127x_dio1, INPUT);

  pinMode(SX127xDriver::SX127x_MOSI, OUTPUT);
  pinMode(SX127xDriver::SX127x_MISO, INPUT);
  pinMode(SX127xDriver::SX127x_SCK, OUTPUT);
  pinMode(SX127xDriver::SX127x_RST, OUTPUT);

  digitalWrite(SX127xDriver::SX127x_nss, HIGH);
  //SPI.setHost(VSPI_HOST);
  //iSPI.init(SX127xDriver::SX127x_MOSI, SX127xDriver::SX127x_MISO, SX127xDriver::SX127x_SCK, -1);
  SPI.begin(SX127xDriver::SX127x_SCK, SX127xDriver::SX127x_MISO, SX127xDriver::SX127x_MOSI, -1); // sck, miso, mosi, ss (ss can be any GPIO)
  SPI.setFrequency(10000000);
  SPI.setBitOrder(MSBFIRST);
  //Divide the clock frequency
  //SPI.setClockDivider(SPI_CLOCK_DIV2);
  //Set data mode
  SPI.setDataMode(SPI_MODE0);
#else
  //  pinMode(SX127xDriver::SX127x_nss, OUTPUT);
  // pinMode(SX127xDriver::SX127x_dio0, INPUT);
  // pinMode(SX127xDriver::SX127x_dio1, INPUT);
  // pinMode(SX127xDriver::SX127x_RST, OUTPUT);
  // digitalWrite(SX127xDriver::SX127x_nss, HIGH);

  SPI.pins(SX127xDriver::SX127x_SCK, SX127xDriver::SX127x_MISO, SX127xDriver::SX127x_MOSI, -1);
  pinMode(SX127xDriver::SX127x_nss, OUTPUT);
  pinMode(SX127xDriver::SX127x_dio0, INPUT);
  pinMode(SX127xDriver::SX127x_dio1, INPUT);

  pinMode(SX127xDriver::SX127x_MOSI, OUTPUT);
  pinMode(SX127xDriver::SX127x_MISO, INPUT);
  pinMode(SX127xDriver::SX127x_SCK, OUTPUT);
  pinMode(SX127xDriver::SX127x_RST, OUTPUT);

  digitalWrite(SX127xDriver::SX127x_nss, HIGH);
  SPI.begin();
  SPI.setBitOrder(MSBFIRST);
  SPI.setDataMode(SPI_MODE0);
  SPI.setFrequency(5000000);
#endif
}

uint8_t ICACHE_RAM_ATTR getRegValue(uint8_t reg, uint8_t msb, uint8_t lsb)
{
  if ((msb > 7) || (lsb > 7) || (lsb > msb))
  {
    return (ERR_INVALID_BIT_RANGE);
  }
  uint8_t rawValue = readRegister(reg);
  uint8_t maskedValue = rawValue & ((0b11111111 << lsb) & (0b11111111 >> (7 - msb)));
  return (maskedValue);
}

uint8_t ICACHE_RAM_ATTR readRegisterBurst(uint8_t reg, uint8_t numBytes, uint8_t *inBytes)
{
  char OutByte;
  digitalWrite(SX127xDriver::SX127x_nss, LOW);
  ////iSPI.beginTransaction(SPISettings(9000000, MSBFIRST, SPI_MODE0));
  OutByte = (reg | SPI_READ);

#ifdef PLATFORM_ESP32
  //iSPI.transferByte(OutByte);
  SPI.write(OutByte);
#else
  SPI.write(OutByte);
#endif

  for (uint8_t i = 0; i < numBytes; i++)
  {
#ifdef PLATFORM_ESP32
    //inBytes[i] = iSPI.transferByte(reg);
    inBytes[i] = SPI.transfer(reg);
#else
    inBytes[i] = SPI.transfer(reg);
#endif
  }
  ////////iSPI.endTransaction();
  digitalWrite(SX127xDriver::SX127x_nss, HIGH);

  if (DebugVerbosity >= DEBUG_4)
  {
    Serial.print("SPI: Read Burst ");
    Serial.print("REG: ");
    Serial.print(reg);
    Serial.print(" LEN: ");
    Serial.print(numBytes);
    Serial.print(" DATA: ");

    for (int i = 0; i < numBytes; i++)
    {
      Serial.print(inBytes[i]);
    }

    Serial.println();
  }

  return (ERR_NONE);
}

//void IRAM_ATTR writeRegisterBurst(uint8_t reg, const volatile uint8_t * data, const volatile uint8_t numBytes) {

// uint8_t IRAM_ATTR readRegisterBurst(uint8_t reg, const volatile uint8_t numBytes, volatile uint8_t * inBytes){
//   char OutByte;
//   digitalWrite(SX127xDriver::SX127x_nss, LOW);
//   ////iSPI.beginTransaction(SPISettings(9000000, MSBFIRST, SPI_MODE0));
//   OutByte = (reg | SPI_READ);
//   SPI.transferByte(OutByte);
//   for (uint8_t i = 0; i < numBytes; i++) {
//     inBytes[i] = SPI.transferByte(reg);
//   }
//   ////////iSPI.endTransaction();
//   digitalWrite(SX127xDriver::SX127x_nss, HIGH);

//   if (DebugVerbosity >= DEBUG_4) {
//     Serial.print("SPI: Read Burst ");
//     Serial.print("REG: ");
//     Serial.print(reg);
//     Serial.print(" LEN: ");
//     Serial.print(numBytes);
//     Serial.print(" DATA: ");

//     for (int i = 0; i < numBytes; i++) {
//       Serial.print(inBytes[i]);
//     }

//     Serial.println();
//   }

//   return (ERR_NONE);
// }

uint8_t ICACHE_RAM_ATTR readRegisterBurst(uint8_t reg, uint8_t numBytes, volatile uint8_t *inBytes)
{
  char OutByte;
  digitalWrite(SX127xDriver::SX127x_nss, LOW);
  ////iSPI.beginTransaction(SPISettings(9000000, MSBFIRST, SPI_MODE0));
  OutByte = (reg | SPI_READ);

#ifdef PLATFORM_ESP32
  //iSPI.transferByte(OutByte);
  SPI.write(OutByte);
#else
  SPI.write(OutByte);
#endif

  for (uint8_t i = 0; i < numBytes; i++)
  {
#ifdef PLATFORM_ESP32
    //inBytes[i] = iSPI.transferByte(reg);
    inBytes[i] = SPI.transfer(reg);
#else
    inBytes[i] = SPI.transfer(reg);
#endif
  }
  ////////iSPI.endTransaction();
  digitalWrite(SX127xDriver::SX127x_nss, HIGH);

  if (DebugVerbosity >= DEBUG_4)
  {
    Serial.print("SPI: Read Burst ");
    Serial.print("REG: ");
    Serial.print(reg);
    Serial.print(" LEN: ");
    Serial.print(numBytes);
    Serial.print(" DATA: ");

    for (int i = 0; i < numBytes; i++)
    {
      Serial.print(inBytes[i]);
    }

    Serial.println();
  }

  return (ERR_NONE);
}

uint8_t ICACHE_RAM_ATTR readRegisterBurst(uint8_t reg, uint8_t numBytes, char *inBytes)
{
  digitalWrite(SX127xDriver::SX127x_nss, LOW);
  //iSPI.beginTransaction(SPISettings(9000000, MSBFIRST, SPI_MODE0));

#ifdef PLATFORM_ESP32
  //iSPI.transferByte(reg | SPI_READ);
  SPI.write(reg | SPI_READ);
#else
  SPI.write(reg | SPI_READ);
#endif

  for (uint8_t i = 0; i < numBytes; i++)
  {
#ifdef PLATFORM_ESP32
    //inBytes[i] = iSPI.transferByte(reg);
    inBytes[i] = SPI.transfer(reg);
#else
    inBytes[i] = SPI.transfer(reg);
#endif
    //Serial.println(inBytes[i]);
  }
  //////iSPI.endTransaction();
  digitalWrite(SX127xDriver::SX127x_nss, HIGH);

  if (DebugVerbosity >= DEBUG_4)
  {
    Serial.print("SPI: Read BurstStr ");
    Serial.print("REG: ");
    Serial.print(reg);
    Serial.print(" LEN: ");
    Serial.print(numBytes);
    Serial.print(" DATA: ");

    for (int i = 0; i < numBytes; i++)
    {
      Serial.print(inBytes[i]);
    }

    Serial.println();
  }

  return (ERR_NONE);
}

uint8_t ICACHE_RAM_ATTR readRegister(uint8_t reg)
{
  uint8_t InByte;
  uint8_t OutByte;
  digitalWrite(SX127xDriver::SX127x_nss, LOW);
  //SPI.beginTransaction(SPISettings(5000000, MSBFIRST, SPI_MODE0));
  OutByte = (reg | SPI_READ);
  //Serial.println(OutByte,HEX);

#ifdef PLATFORM_ESP32
  //iSPI.transferByte(OutByte);
  SPI.write(OutByte);
#else
  SPI.write(OutByte);
#endif

  //Serial.println(OutByte,HEX);

#ifdef PLATFORM_ESP32
  //InByte = iSPI.transferByte(0x00);
  InByte = SPI.transfer(0x00);
#else
  InByte = SPI.transfer(0x00);
#endif

  //iSPI.sendByte(&OutByte, &InByte, (uint8_t)1);

  //SPI.endTransaction();
  digitalWrite(SX127xDriver::SX127x_nss, HIGH);

  //  if (1 == 1) {
  //    Serial.print("SPI: Read ");
  //    Serial.print("REG: ");
  //    Serial.print(reg);
  //    Serial.print(" VAL: ");
  //    Serial.println(InByte, HEX);
  //  }
  return (InByte);
}

uint8_t ICACHE_RAM_ATTR setRegValue(uint8_t reg, uint8_t value, uint8_t msb, uint8_t lsb)
{
  if ((msb > 7) || (lsb > 7) || (lsb > msb))
  {
    return (ERR_INVALID_BIT_RANGE);
  }

  uint8_t currentValue = readRegister(reg);
  uint8_t mask = ~((0b11111111 << (msb + 1)) | (0b11111111 >> (8 - lsb)));
  uint8_t newValue = (currentValue & ~mask) | (value & mask);
  writeRegister(reg, newValue);
  return (ERR_NONE);
}

// void ICACHE_RAM_ATTR writeRegisterFIFO(uint8_t reg, volatile uint8_t *data, uint8_t length)
// {
//   uint8_t DataOut[length + 1];
//   uint8_t Datain[1];

//   for (int i = 0; i < length; i++) // check if I need this!!!
//   {
//     DataOut[i + 1] = data[i];
//   }

//   DataOut[0] = (0x00 | 0b10000000);

//   digitalWrite(SX127xDriver::SX127x_nss, LOW);
//   //SPI.transferByte(reg | SPI_WRITE);
// #ifdef PLATFORM_ESP32
//   //iSPI.transferByteFIFO(data, length);
//   SPI.transferBytes((uint8_t *)DataOut, NULL, length);
// #else
//   SPI.transferBytes((uint8_t *)NULL, (uint8_t *)data, length);
// #endif

//   digitalWrite(SX127xDriver::SX127x_nss, HIGH);
// }

// void ICACHE_RAM_ATTR writeRegisterBurst(uint8_t reg, uint8_t *data, uint8_t numBytes)
// {
//   digitalWrite(SX127xDriver::SX127x_nss, LOW);
//   ////iSPI.beginTransaction(SPISettings(9000000, MSBFIRST, SPI_MODE0));
// #ifdef PLATFORM_ESP32
//   //iSPI.transferByte(reg | SPI_WRITE);
//   SPI.write(reg | SPI_WRITE);
// #else
//   iSPI.transfer(reg | SPI_WRITE);
// #endif

//   //SPDR = (reg | SPI_WRITE);
//   for (uint8_t i = 0; i < numBytes; i++)
//   {
// #ifdef PLATFORM_ESP32
//     //iSPI.transferByte(data[i]);
//     SPI.write(data[i]);
// #else
//     iSPI.transfer(data[i]);
// #endif
//   }
//   //////iSPI.endTransaction();
//   digitalWrite(SX127xDriver::SX127x_nss, HIGH);
// }

// void ICACHE_RAM_ATTR writeRegisterBurst(uint8_t reg, const volatile uint8_t *data, const volatile uint8_t numBytes)
// {
//   digitalWrite(SX127xDriver::SX127x_nss, LOW);
//   ////iSPI.beginTransaction(SPISettings(9000000, MSBFIRST, SPI_MODE0));
// #ifdef PLATFORM_ESP32
//   //iSPI.transferByte(reg | SPI_WRITE);
//   SPI.write(reg | SPI_WRITE);
// #else
//   iSPI.transfer(reg | SPI_WRITE);
// #endif

//   //SPDR = (reg | SPI_WRITE);
//   for (uint8_t i = 0; i < numBytes; i++)
//   {
// #ifdef PLATFORM_ESP32
//     //iSPI.transferByte(data[i]);
//     SPI.write(data[i]);
// #else
//     iSPI.transfer(data[i]);
// #endif
//   }
//   //////iSPI.endTransaction();
//   Serial.println("Sent Burst String writeRegisterBurst");
//   digitalWrite(SX127xDriver::SX127x_nss, HIGH);
// }

void ICACHE_RAM_ATTR writeRegisterBurstStr(uint8_t reg, uint8_t *data, uint8_t numBytes)
{
  digitalWrite(SX127xDriver::SX127x_nss, LOW);
  ////iSPI.beginTransaction(SPISettings(9000000, MSBFIRST, SPI_MODE0));

  // uint8_t DataOut[numBytes + 1];
  // uint8_t Datain[1];

  // for (int i = 0; i < numBytes; i++) // check if I need this!!!
  // {
  //   DataOut[i + 1] = data[i];
  // }

  // DataOut[0] = (0x00 | 0b10000000);

  // #ifdef PLATFORM_ESP32
  //   //iSPI.transferByte(reg | SPI_WRITE);
  //SPI.write(reg | SPI_WRITE);
  // #else
  //   iSPI.transfer(reg | SPI_WRITE);
  // #endif

  //SPI.transferBytes(DataOut, numBytes);
  //SPI.write(reg | SPI_WRITE);

  for (uint8_t i = 0; i < numBytes; i++)
  {
#ifdef PLATFORM_ESP32
    //iSPI.transferByte(data[i]);
    //writeRegister(reg | SPI_WRITE, data[i]);
    SPI.write(reg | SPI_WRITE);
    SPI.write(data[i]);
#else
    //iSPI.transfer(data[i]);
    SPI.write(reg | SPI_WRITE);
    SPI.write(data[i]);
#endif
  }
  //////iSPI.endTransaction();
  // Serial.print("Sent Burst String writeRegisterBurst non-const:");
  // for (uint8_t i = 0; i < numBytes; i++)
  // {
  //   Serial.print(data[i]);
  // }
  // Serial.println();

  digitalWrite(SX127xDriver::SX127x_nss, HIGH);
}

void ICACHE_RAM_ATTR writeRegisterBurstStr(uint8_t reg, const volatile uint8_t *data, uint8_t numBytes)
{
  digitalWrite(SX127xDriver::SX127x_nss, LOW);
  ////iSPI.beginTransaction(SPISettings(9000000, MSBFIRST, SPI_MODE0));

  SPI.write(reg | SPI_WRITE);
  SPI.writeBytes((uint8_t *)data, numBytes);

  //////iSPI.endTransaction();
  digitalWrite(SX127xDriver::SX127x_nss, HIGH);
}

void ICACHE_RAM_ATTR writeRegister(uint8_t reg, uint8_t data)
{
  digitalWrite(SX127xDriver::SX127x_nss, LOW);
  //  iSPI.beginTransaction(SPISettings(9000000, MSBFIRST, SPI_MODE0));
#ifdef PLATFORM_ESP32
  //iSPI.transferByte(reg | SPI_WRITE);
  SPI.write(reg | SPI_WRITE);
#else
  SPI.write(reg | SPI_WRITE);
#endif

#ifdef PLATFORM_ESP32
  //iSPI.transferByte(data);
  SPI.write(data);
#else
  SPI.write(data);
#endif
  //  iSPI.endTransaction();
  digitalWrite(SX127xDriver::SX127x_nss, HIGH);

  if (DebugVerbosity >= DEBUG_4)
  {
    Serial.print("SPI: Write ");
    Serial.print("REG: ");
    Serial.print(reg, HEX);
    Serial.print(" VAL: ");
    Serial.println(data, HEX);
  }
}

//uint8_t IRAM_ATTR readRegisterBurst(uint8_t reg, uint8_t numBytes, uint8_t* inBytes) {
//
//  digitalWrite(SX127xDriver::SX127x_nss, LOW);
//  ////SPI.beginTransaction(SPISettings(9000000, MSBFIRST, SPI_MODE0));
//  SPI.transfer(reg | SPI_READ);
//  for (uint8_t i = 0; i < numBytes; i++) {
//    inBytes[i] = SPI.transfer(reg);
//  }
//  ////////SPI.endTransaction();
//  digitalWrite(SX127xDriver::SX127x_nss, HIGH);
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
//  digitalWrite(SX127xDriver::SX127x_nss, LOW);
//  //SPI.beginTransaction(SPISettings(9000000, MSBFIRST, SPI_MODE0));
//  SPI.transfer(reg | SPI_READ);
//  for (uint8_t i = 0; i < numBytes; i++) {
//    inBytes[i] = SPI.transfer(reg);
//    //Serial.println(inBytes[i]);
//  }
//  //////SPI.endTransaction();
//  digitalWrite(SX127xDriver::SX127x_nss, HIGH);
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
//  digitalWrite(SX127xDriver::SX127x_nss, LOW);
//  SPI.beginTransaction(SPISettings(9000000, MSBFIRST, SPI_MODE0));
//  SPI.transfer(reg | SPI_READ);
//  inByte = SPI.transfer(0x00);
//  SPI.endTransaction();
//  digitalWrite(SX127xDriver::SX127x_nss, HIGH);
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
//  digitalWrite(SX127xDriver::SX127x_nss, LOW);
//  ////SPI.beginTransaction(SPISettings(9000000, MSBFIRST, SPI_MODE0));
//  SPI.transfer(reg | SPI_WRITE);
//  //SPDR = (reg | SPI_WRITE);
//  for (uint8_t i = 0; i < numBytes; i++) {
//    SPI.transfer(data[i]);
//  }
//  //////SPI.endTransaction();
//  digitalWrite(SX127xDriver::SX127x_nss, HIGH);
//
//
//
//}
//
//void IRAM_ATTR writeRegisterBurstStr(uint8_t reg, const char* data, uint8_t numBytes) {
//  digitalWrite(SX127xDriver::SX127x_nss, LOW);
//  ////SPI.beginTransaction(SPISettings(9000000, MSBFIRST, SPI_MODE0));
//  SPI.transfer(reg | SPI_WRITE);
//  for (uint8_t i = 0; i < numBytes; i++) {
//    SPI.transfer(data[i]);
//  }
//  //////SPI.endTransaction();
//  digitalWrite(SX127xDriver::SX127x_nss, HIGH);
//}
//
//void IRAM_ATTR writeRegister(uint8_t reg, uint8_t data) {
//  digitalWrite(SX127xDriver::SX127x_nss, LOW);
//  SPI.beginTransaction(SPISettings(9000000, MSBFIRST, SPI_MODE0));
//  SPI.transfer(reg | SPI_WRITE);
//  SPI.transfer(data);
//  SPI.endTransaction();
//  digitalWrite(SX127xDriver::SX127x_nss, HIGH);
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

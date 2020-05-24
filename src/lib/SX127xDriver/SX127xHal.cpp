#include "SX127xHal.h"

SX127xHal *SX127xHal::instance = NULL;

SX127xHal::SX127xHal(int MISO, int MOSI, int SCK, int NSS, int RST, int DIO0, int DIO1, int RXenb, int TXenb)
{
  instance = this;

  GPIO_MOSI = MOSI;
  GPIO_MISO = MISO;
  GPIO_SCK = SCK;
  GPIO_NSS = NSS;
  GPIO_RST = RST;

  GPIO_DIO0 = DIO0;
  GPIO_DIO1 = DIO1;

  if ((GPIO_RXenb < 0) && (GPIO_TXenb < 0))
  {
    UsePApins = true;
    GPIO_RXenb = RXenb;
    GPIO_TXenb = TXenb;
  }
}

void SX127xHal::init()
{
  Serial.print("MISO:");
  Serial.print(GPIO_MOSI);

  Serial.print(" MOSI:");
  Serial.print(GPIO_MISO);

  Serial.print(" SCK:");
  Serial.print(GPIO_SCK);

  Serial.print(" NSS:");
  Serial.print(GPIO_NSS);

  Serial.print(" RESET:");
  Serial.print(GPIO_RST);

  Serial.print(" DIO0:");
  Serial.print(GPIO_DIO0);

  Serial.print(" DIO1:");
  Serial.print(GPIO_DIO1);

  Serial.print(" RXenb:");
  Serial.print(GPIO_RXenb);

  Serial.print(" TXenb:");
  Serial.println(GPIO_TXenb);

  if (UsePApins)
  {
    pinMode(GPIO_RXenb, OUTPUT);
    pinMode(GPIO_TXenb, OUTPUT);
    digitalWrite(GPIO_RXenb, LOW);
    digitalWrite(GPIO_TXenb, LOW);
  }

#ifdef PLATFORM_ESP32
  SPI.begin(GPIO_SCK, GPIO_MISO, GPIO_MOSI); // sck, miso, mosi, ss (ss can be any GPIO)
  SPI.setBitOrder(MSBFIRST);
  SPI.setDataMode(SPI_MODE0);
  SPI.setFrequency(10000000);
#endif

#ifdef PLATFORM_ESP8266
  SPI.begin();
  SPI.setBitOrder(MSBFIRST);
  SPI.setDataMode(SPI_MODE0);
  SPI.setFrequency(10000000);
#endif

#ifdef PLATFORM_STM32
  SPI.setMOSI(GPIO_MOSI);
  SPI.setMISO(GPIO_MISO);
  SPI.setSCLK(GPIO_SCK);
  SPI.setBitOrder(MSBFIRST);
  SPI.setDataMode(SPI_MODE0);
  SPI.begin();
  SPI.setClockDivider(SPI_CLOCK_DIV4); // 72 / 8 = 9 MHz
#endif

  pinMode(GPIO_NSS, OUTPUT);
  pinMode(GPIO_RST, OUTPUT);

  pinMode(GPIO_DIO0, INPUT);
  pinMode(GPIO_DIO1, INPUT);

  //pinMode(GPIO_MOSI, OUTPUT);
  //pinMode(GPIO_MISO, INPUT);
  //pinMode(GPIO_SCK, OUTPUT);

  digitalWrite(GPIO_NSS, HIGH);

  delay(100);
  digitalWrite(GPIO_RST, 0);
  delay(100);
  digitalWrite(GPIO_RST, 1);
  delay(100);
}

uint8_t ICACHE_RAM_ATTR SX127xHal::getRegValue(uint8_t reg, uint8_t msb, uint8_t lsb)
{
  if ((msb > 7) || (lsb > 7) || (lsb > msb))
  {
    return (ERR_INVALID_BIT_RANGE);
  }
  uint8_t rawValue = readRegister(reg);
  uint8_t maskedValue = rawValue & ((0b11111111 << lsb) & (0b11111111 >> (7 - msb)));
  return (maskedValue);
}

uint8_t ICACHE_RAM_ATTR SX127xHal::readRegisterBurst(uint8_t reg, uint8_t numBytes, uint8_t *inBytes)
{
  char OutByte;
  digitalWrite(GPIO_NSS, LOW);
  OutByte = (reg | SPI_READ);

#ifdef PLATFORM_STM32
  SPI.transfer(OutByte);
#else
  SPI.write(OutByte);
#endif

  SPI.transfer(inBytes, numBytes);
  digitalWrite(GPIO_NSS, HIGH);

  return (ERR_NONE);
}

uint8_t ICACHE_RAM_ATTR SX127xHal::readRegister(uint8_t reg)
{
  uint8_t InByte;
  uint8_t OutByte;
  digitalWrite(GPIO_NSS, LOW);

  OutByte = (reg | SPI_READ);

#ifdef PLATFORM_STM32
  SPI.transfer(OutByte);
#else
  SPI.write(OutByte);
#endif

  InByte = SPI.transfer(0x00);

  digitalWrite(GPIO_NSS, HIGH);

  return (InByte);
}

uint8_t ICACHE_RAM_ATTR SX127xHal::setRegValue(uint8_t reg, uint8_t value, uint8_t msb, uint8_t lsb)
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

void ICACHE_RAM_ATTR SX127xHal::writeRegisterBurst(uint8_t reg, uint8_t *data, uint8_t numBytes)
{
  digitalWrite(GPIO_NSS, LOW);

#ifdef PLATFORM_STM32
  SPI.transfer(reg | SPI_WRITE);
  SPI.transfer(data, numBytes);
#else
  SPI.write(reg | SPI_WRITE);
  SPI.writeBytes(data, numBytes);
#endif

  digitalWrite(GPIO_NSS, HIGH);
}

void ICACHE_RAM_ATTR SX127xHal::writeRegister(uint8_t reg, uint8_t data)
{
  digitalWrite(GPIO_NSS, LOW);

#ifdef PLATFORM_STM32
  SPI.transfer(reg | SPI_WRITE);
  SPI.transfer(data);
#else
  SPI.write(reg | SPI_WRITE);
  SPI.write(data);
#endif

  digitalWrite(GPIO_NSS, HIGH);
}

void ICACHE_RAM_ATTR SX127xHal::TXenable()
{
}
void ICACHE_RAM_ATTR SX127xHal::RXenable()
{
}
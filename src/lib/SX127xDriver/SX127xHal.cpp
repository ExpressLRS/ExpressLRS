#include "SX127xHal.h"

SX127xHal *SX127xHal::instance = NULL;

void inline SX127xHal::nullCallback(void) { return; };
void (*SX127xHal::TXdoneCallback)() = &nullCallback;
void (*SX127xHal::RXdoneCallback)() = &nullCallback;

SX127xHal::SX127xHal()
{
  instance = this;
}

void SX127xHal::end()
{
  SPI.end();
  detachInterrupt(GPIO_PIN_DIO0);
}

void SX127xHal::init()
{
  Serial.println("Hal Init");

#if defined(GPIO_PIN_RX_ENABLE) || defined(GPIO_PIN_TX_ENABLE)
  Serial.print("This Target uses seperate TX/RX enable pins: ");
#endif

#if defined(GPIO_PIN_TX_ENABLE)
  Serial.print("TX: ");
  Serial.print(GPIO_PIN_TX_ENABLE);
#endif

#if defined(GPIO_PIN_RX_ENABLE)
  Serial.print(" RX: ");
  Serial.println(GPIO_PIN_RX_ENABLE);
#endif

#if defined(GPIO_PIN_RX_ENABLE)
  pinMode(GPIO_PIN_RX_ENABLE, OUTPUT);
  digitalWrite(GPIO_PIN_RX_ENABLE, LOW);
#endif

#if defined(GPIO_PIN_TX_ENABLE)
  pinMode(GPIO_PIN_TX_ENABLE, OUTPUT);
  digitalWrite(GPIO_PIN_TX_ENABLE, LOW);
#endif

#ifdef PLATFORM_ESP32
  SPI.begin(GPIO_PIN_SCK, GPIO_PIN_MISO, GPIO_PIN_MOSI); // sck, miso, mosi, ss (ss can be any GPIO)
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
  SPI.setMOSI(GPIO_PIN_MOSI);
  SPI.setMISO(GPIO_PIN_MISO);
  SPI.setSCLK(GPIO_PIN_SCK);
  SPI.setBitOrder(MSBFIRST);
  SPI.setDataMode(SPI_MODE0);
  SPI.setClockDivider(SPI_CLOCK_DIV4); // 72 / 8 = 9 MHz
  SPI.begin();                         // SPI.setFrequency(10000000);
#endif

  pinMode(GPIO_PIN_NSS, OUTPUT);
  pinMode(GPIO_PIN_RST, OUTPUT);
  pinMode(GPIO_PIN_DIO0, INPUT);

  digitalWrite(GPIO_PIN_NSS, HIGH);

  delay(100);
  digitalWrite(GPIO_PIN_RST, 0);
  delay(100);
  pinMode(GPIO_PIN_RST, INPUT); // leave floating

  attachInterrupt(digitalPinToInterrupt(GPIO_PIN_DIO0), dioISR, RISING);
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
  digitalWrite(GPIO_PIN_NSS, LOW);
  OutByte = (reg | SPI_READ);

#ifdef PLATFORM_STM32
  SPI.transfer(OutByte);
#else
  SPI.write(OutByte);
#endif

  SPI.transfer(inBytes, numBytes);
  digitalWrite(GPIO_PIN_NSS, HIGH);

  return (ERR_NONE);
}

uint8_t ICACHE_RAM_ATTR SX127xHal::readRegister(uint8_t reg)
{
  uint8_t InByte;
  uint8_t OutByte;
  digitalWrite(GPIO_PIN_NSS, LOW);

  OutByte = (reg | SPI_READ);

#ifdef PLATFORM_STM32
  SPI.transfer(OutByte);
#else
  SPI.write(OutByte);
#endif

  InByte = SPI.transfer(0x00);

  digitalWrite(GPIO_PIN_NSS, HIGH);

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

void ICACHE_RAM_ATTR SX127xHal::writeRegisterFIFO(volatile uint8_t *data, uint8_t numBytes)
{
  uint8_t localbuf[numBytes];

  for (int i = 0; i < numBytes; i++) // todo check if this is the right want to handle volatiles
  {
    localbuf[i] = data[i];
  }

  digitalWrite(GPIO_PIN_NSS, LOW);

#ifdef PLATFORM_STM32
  SPI.transfer(SX127X_REG_FIFO | SPI_WRITE);
  SPI.transfer(localbuf, numBytes);
#else
  SPI.write(SX127X_REG_FIFO | SPI_WRITE);
  SPI.writeBytes(localbuf, numBytes);
#endif

  digitalWrite(GPIO_PIN_NSS, HIGH);
}

void ICACHE_RAM_ATTR SX127xHal::readRegisterFIFO(volatile uint8_t *data, uint8_t numBytes)
{
  uint8_t localbuf[numBytes];
  digitalWrite(GPIO_PIN_NSS, LOW);

#ifdef PLATFORM_STM32
  SPI.transfer(SX127X_REG_FIFO | SPI_READ);
#else
  SPI.write(SX127X_REG_FIFO | SPI_READ);
#endif

  SPI.transfer(localbuf, numBytes);
  digitalWrite(GPIO_PIN_NSS, HIGH);

  for (int i = 0; i < numBytes; i++) // todo check if this is the right want to handle volatiles
  {
    data[i] = localbuf[i];
  }
}

void ICACHE_RAM_ATTR SX127xHal::writeRegisterBurst(uint8_t reg, uint8_t *data, uint8_t numBytes)
{
  digitalWrite(GPIO_PIN_NSS, LOW);

#ifdef PLATFORM_STM32
  SPI.transfer(reg | SPI_WRITE);
  SPI.transfer(data, numBytes);
#else
  SPI.write(reg | SPI_WRITE);
  SPI.writeBytes(data, numBytes);
#endif

  digitalWrite(GPIO_PIN_NSS, HIGH);
}

void ICACHE_RAM_ATTR SX127xHal::writeRegister(uint8_t reg, uint8_t data)
{
  digitalWrite(GPIO_PIN_NSS, LOW);

#ifdef PLATFORM_STM32
  SPI.transfer(reg | SPI_WRITE);
  SPI.transfer(data);
#else
  SPI.write(reg | SPI_WRITE);
  SPI.write(data);
#endif

  digitalWrite(GPIO_PIN_NSS, HIGH);
}

void ICACHE_RAM_ATTR SX127xHal::TXenable()
{
  InterruptAssignment = SX127x_INTERRUPT_TX_DONE;
#if defined(GPIO_PIN_RX_ENABLE)
  digitalWrite(GPIO_PIN_RX_ENABLE, LOW);
#endif

#if defined(GPIO_PIN_TX_ENABLE)
  digitalWrite(GPIO_PIN_TX_ENABLE, HIGH);
#endif
  return;
}

void ICACHE_RAM_ATTR SX127xHal::RXenable()
{
  InterruptAssignment = SX127x_INTERRUPT_RX_DONE;

#if defined(GPIO_PIN_RX_ENABLE)
  digitalWrite(GPIO_PIN_RX_ENABLE, HIGH);
#endif

#if defined(GPIO_PIN_TX_ENABLE)
  digitalWrite(GPIO_PIN_TX_ENABLE, LOW);
#endif
  return;
}

void ICACHE_RAM_ATTR SX127xHal::TXRXdisable()
{
#if defined(GPIO_PIN_RX_ENABLE)
  digitalWrite(GPIO_PIN_RX_ENABLE, LOW);
#endif

#if defined(GPIO_PIN_TX_ENABLE)
  digitalWrite(GPIO_PIN_TX_ENABLE, LOW);
#endif
  return;
}

void ICACHE_RAM_ATTR SX127xHal::dioISR()
{
  if (instance->InterruptAssignment == SX127x_INTERRUPT_TX_DONE)
  {
    TXdoneCallback();
  }
  else if (instance->InterruptAssignment == SX127x_INTERRUPT_RX_DONE)
  {
    RXdoneCallback();
  }
}
#include "SX127xHal.h"

SX127xHal *SX127xHal::instance = NULL;

volatile SX127x_InterruptAssignment SX127xHal::InterruptAssignment = SX127x_INTERRUPT_NONE;

void inline SX127xHal::nullCallback(void) { return; }
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
  pinMode(GPIO_PIN_TX_ENABLE, OUTPUT);
  digitalWrite(GPIO_PIN_TX_ENABLE, LOW);
#endif

#if defined(GPIO_PIN_RX_ENABLE)
  Serial.print(" RX: ");
  Serial.println(GPIO_PIN_RX_ENABLE);
  pinMode(GPIO_PIN_RX_ENABLE, OUTPUT);
  digitalWrite(GPIO_PIN_RX_ENABLE, LOW);
#endif

#if defined(GPIO_PIN_ANTENNA_SELECT)
  pinMode(GPIO_PIN_ANTENNA_SELECT, OUTPUT);
  digitalWrite(GPIO_PIN_ANTENNA_SELECT, LOW);
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

void ICACHE_RAM_ATTR SX127xHal::readRegisterBurst(uint8_t reg, uint8_t numBytes, uint8_t *inBytes)
{
  WORD_ALIGNED_ATTR uint8_t buf[numBytes + 1];
  buf[0] = reg | SPI_READ;

  digitalWrite(GPIO_PIN_NSS, LOW);
  SPI.transfer(buf, numBytes + 1);
  digitalWrite(GPIO_PIN_NSS, HIGH);

  memcpy(inBytes, buf + 1, numBytes);
}

uint8_t ICACHE_RAM_ATTR SX127xHal::readRegister(uint8_t reg)
{
  WORD_ALIGNED_ATTR uint8_t buf[2];

  buf[0] = reg | SPI_READ;

  digitalWrite(GPIO_PIN_NSS, LOW);
  SPI.transfer(buf, 2);
  digitalWrite(GPIO_PIN_NSS, HIGH);

  return (buf[1]);
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
  WORD_ALIGNED_ATTR uint8_t buf[numBytes + 1];
  buf[0] = (SX127X_REG_FIFO | SPI_WRITE);

  for (int i = 0; i < numBytes; i++) // todo check if this is the right want to handle volatiles
  {
    buf[i + 1] = data[i];
  }

  digitalWrite(GPIO_PIN_NSS, LOW);
#ifdef PLATFORM_STM32
  SPI.transfer(buf, numBytes + 1);
#else
  SPI.writeBytes(buf, numBytes + 1);
#endif
  digitalWrite(GPIO_PIN_NSS, HIGH);
}

void ICACHE_RAM_ATTR SX127xHal::readRegisterFIFO(volatile uint8_t *data, uint8_t numBytes)
{
  WORD_ALIGNED_ATTR uint8_t buf[numBytes + 1];
  buf[0] = SX127X_REG_FIFO | SPI_READ;

  digitalWrite(GPIO_PIN_NSS, LOW);
  SPI.transfer(buf, numBytes + 1);
  digitalWrite(GPIO_PIN_NSS, HIGH);

  for (int i = 0; i < numBytes; i++) // todo check if this is the right want to handle volatiles
  {
    data[i] = buf[i + 1];
  }
}

void ICACHE_RAM_ATTR SX127xHal::writeRegisterBurst(uint8_t reg, uint8_t *data, uint8_t numBytes)
{
  WORD_ALIGNED_ATTR uint8_t buf[numBytes + 1];
  buf[0] = reg | SPI_WRITE;
  memcpy(buf + 1,  data, numBytes);

  digitalWrite(GPIO_PIN_NSS, LOW);
#ifdef PLATFORM_STM32
  SPI.transfer(buf, numBytes + 1);
#else
  SPI.writeBytes(buf, numBytes + 1);
#endif
  digitalWrite(GPIO_PIN_NSS, HIGH);
}

void ICACHE_RAM_ATTR SX127xHal::writeRegister(uint8_t reg, uint8_t data)
{
  WORD_ALIGNED_ATTR uint8_t buf[2];

  buf[0] = reg | SPI_WRITE;
  buf[1] = data;

  digitalWrite(GPIO_PIN_NSS, LOW);
#ifdef PLATFORM_STM32
  SPI.transfer(buf, 2);
#else
  SPI.writeBytes(buf, 2);
#endif
  digitalWrite(GPIO_PIN_NSS, HIGH);
}

void ICACHE_RAM_ATTR SX127xHal::TXenable()
{
  instance->InterruptAssignment = SX127x_INTERRUPT_TX_DONE;
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
  instance->InterruptAssignment = SX127x_INTERRUPT_RX_DONE;

#if defined(GPIO_PIN_RX_ENABLE)
  digitalWrite(GPIO_PIN_RX_ENABLE, HIGH);
#endif

#if defined(GPIO_PIN_TX_ENABLE)
  digitalWrite(GPIO_PIN_TX_ENABLE, LOW);
#endif
}

void ICACHE_RAM_ATTR SX127xHal::TXRXdisable()
{
  instance->InterruptAssignment = SX127x_INTERRUPT_NONE;
#if defined(GPIO_PIN_RX_ENABLE)
  digitalWrite(GPIO_PIN_RX_ENABLE, LOW);
#endif

#if defined(GPIO_PIN_TX_ENABLE)
  digitalWrite(GPIO_PIN_TX_ENABLE, LOW);
#endif
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

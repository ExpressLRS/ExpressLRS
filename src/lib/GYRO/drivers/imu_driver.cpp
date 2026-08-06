#include "imu_driver.h"


#if defined(GYRO_SUPPORT)

#include <wire.h>
#include "SPI.h"
extern SPIClass _spi;

#define SPI_READ_BIT    0x80

static volatile bool irq_received = false;

static IRAM_ATTR void irq_handler() {
    irq_received = true;
}

void IMU_Driver::setupInterrupt(uint8_t pin) {
    // Setup interrupt
    pinMode(pin, INPUT_PULLUP);
    attachInterrupt(pin, irq_handler, RISING);
}

bool IMU_Driver::interruptReceived() {
    return irq_received;
}

void IMU_Driver::setInterruptReceived(bool val)
{
   irq_received = val; 
}

void IMU_Driver::writeRegisterBits(uint8_t registerID, uint8_t mask, uint8_t value) {
    uint8_t newValue;

    if (readRegister(registerID, &newValue, 1)) {
        delayMicroseconds(2);
        newValue = (newValue & ~mask) | value;
        writeRegister(registerID, newValue);
    }
}


uint8_t IMU_Driver_I2C::readRegister(uint8_t reg) {
    uint8_t data = 0;
    readRegister(reg, &data, 1);
    return data;
}

bool IMU_Driver_I2C::readRegister(uint8_t reg, uint8_t *data, size_t size)
{
    size_t r = 0;
    Wire.beginTransmission(m_address);
    Wire.write(reg);
    if (Wire.endTransmission() == 0)
    {
        Wire.requestFrom(m_address, size);
        r = Wire.readBytes(data, size);
    }
    return r == size;
}

void IMU_Driver_I2C::writeRegister(uint8_t reg, uint8_t value)
{
    uint8_t data = value;
    Wire.beginTransmission(m_address);
    Wire.write(reg);
    Wire.write(&data, 1);
    Wire.endTransmission();
    //return (Wire.endTransmission() == 0);
}

uint8_t IMU_Driver_SPI::readRegister(uint8_t reg) {
  _spi.beginTransaction(_spiSettings);
  digitalWrite(cs_pin, LOW);

  _spi.transfer(reg | SPI_READ_BIT);  //Turn on SPI_READ_BIT
  uint8_t val = _spi.transfer(0xFF);

  digitalWrite(cs_pin, HIGH);
  _spi.endTransaction();
  return val;
}

bool IMU_Driver_SPI::readRegister(uint8_t reg, uint8_t *data, size_t size) {
   _spi.beginTransaction(_spiSettings);
  digitalWrite(cs_pin, LOW);

  // IF_INC (auto-increment) needs to be setup in CTRL3_C
  _spi.transfer(reg | SPI_READ_BIT);
  for (uint8_t i = 0; i < size; i++) {
    data[i] = _spi.transfer(0xFF);
  }
  digitalWrite(cs_pin, HIGH);
  _spi.endTransaction();
  return true;
}

void IMU_Driver_SPI::writeRegister(uint8_t reg, uint8_t value) {
  _spi.beginTransaction(_spiSettings);
  digitalWrite(cs_pin, LOW);

  _spi.transfer(reg);         // 写 = 最高位 0
  _spi.transfer(value);

  digitalWrite(cs_pin, HIGH);
  _spi.endTransaction();
}


#endif // GYRO_SUPPORT







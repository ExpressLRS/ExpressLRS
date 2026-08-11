#include "imu_driver.h"

#if defined(PLATFORM_ESP32)

#include "SPI.h"
#include "logging.h"

#include <Wire.h>
extern SPIClass _spi;

#define SPI_READ_BIT 0x80

static volatile bool irq_received = false;

static IRAM_ATTR void irq_handler()
{
    irq_received = true;
}

void IMU_Driver::setupInterrupt(uint8_t pin)
{
    // Setup interrupt
    pinMode(pin, INPUT_PULLUP);
    attachInterrupt(pin, irq_handler, RISING);
}

bool IMU_Driver::interruptReceived()
{
    return irq_received;
}

void IMU_Driver::setInterruptReceived(bool val)
{
    irq_received = val;
}

void IMU_Driver::writeRegisterBits(uint8_t registerID, uint8_t mask, uint8_t value)
{
    uint8_t newValue;

    if (readRegister(registerID, &newValue, 1))
    {
        delayMicroseconds(2);
        newValue = (newValue & ~mask) | value;
        writeRegister(registerID, newValue);
    }
}

bool IMU_Driver_I2C::initialize()
{
    const int gyro_scl = hardware_pin(HARDWARE_gyro_scl);
    const int gyro_sda = hardware_pin(HARDWARE_gyro_sda);
    if (gyro_scl != UNDEF_PIN && gyro_sda != UNDEF_PIN)
    {
        DBGLN("Starting I2C Gyro on SCL %d, SDA %d", gyro_scl, gyro_sda);
        Wire1.begin(gyro_sda, gyro_scl);
        wire = &Wire1;
    }
    else
    {
        DBGLN("Starting I2C Gyro on default I2C pins SCL %d, SDA %d", GPIO_PIN_SCL, GPIO_PIN_SDA);
        wire = &Wire;
    }
    return true;
}

uint8_t IMU_Driver_I2C::readRegister(uint8_t reg)
{
    uint8_t data = 0;
    readRegister(reg, &data, 1);
    return data;
}

bool IMU_Driver_I2C::readRegister(uint8_t reg, uint8_t *data, size_t size)
{
    size_t r = 0;
    wire->beginTransmission(m_address);
    wire->write(reg);
    if (wire->endTransmission() == 0)
    {
        wire->requestFrom(m_address, size);
        r = wire->readBytes(data, size);
    }
    return r == size;
}

void IMU_Driver_I2C::writeRegister(uint8_t reg, uint8_t value)
{
    uint8_t data = value;
    wire->beginTransmission(m_address);
    wire->write(reg);
    wire->write(&data, 1);
    wire->endTransmission();
    // return (wire->endTransmission() == 0);
}

uint8_t IMU_Driver_SPI::readRegister(uint8_t reg)
{
    _spi.beginTransaction(_spiSettings);
    digitalWrite(cs_pin, LOW);

    _spi.transfer(reg | SPI_READ_BIT); // Turn on SPI_READ_BIT
    uint8_t val = _spi.transfer(0xFF);

    digitalWrite(cs_pin, HIGH);
    _spi.endTransaction();
    return val;
}

bool IMU_Driver_SPI::readRegister(uint8_t reg, uint8_t *data, size_t size)
{
    _spi.beginTransaction(_spiSettings);
    digitalWrite(cs_pin, LOW);

    // IF_INC (auto-increment) needs to be setup in CTRL3_C
    _spi.transfer(reg | SPI_READ_BIT);
    for (uint8_t i = 0; i < size; i++)
    {
        data[i] = _spi.transfer(0xFF);
    }
    digitalWrite(cs_pin, HIGH);
    _spi.endTransaction();
    return true;
}

void IMU_Driver_SPI::writeRegister(uint8_t reg, uint8_t value)
{
    _spi.beginTransaction(_spiSettings);
    digitalWrite(cs_pin, LOW);

    _spi.transfer(reg); // 写 = 最高位 0
    _spi.transfer(value);

    digitalWrite(cs_pin, HIGH);
    _spi.endTransaction();
}

#endif 

#pragma once

#include "targets.h"
#include "SPI.h"
#include "Wire.h"

class IMU_Driver
{
public:
    virtual ~IMU_Driver() = default;

    uint16_t gyroSampleRate = 1000;
    uint16_t period_us = 1000;

    float accScaleG, acc1G_adc, gyroScaleRad, gyroScaleDeg;

    virtual const char *GetMPUName() = 0;
    virtual bool initialize() = 0;
    virtual void start() = 0;

    virtual bool isDataReady() = 0;
    virtual bool rawRead(int16_t *ax, int16_t *ay, int16_t *az, int16_t *gx, int16_t *gy, int16_t *gz) = 0;

    virtual uint8_t readRegister(uint8_t reg) = 0;
    virtual bool readRegister(uint8_t reg, uint8_t *data, size_t size) = 0;
    virtual void writeRegister(uint8_t reg, uint8_t value) = 0;
    void writeRegisterBits(uint8_t registerID, uint8_t mask, uint8_t value);
    void setupInterrupt(uint8_t pin);
    bool interruptReceived();
    void setInterruptReceived(bool val);

    long non_ready_errors = 0;
};

class IMU_Driver_I2C : public IMU_Driver
{
public:
    ~IMU_Driver_I2C() override = default;

    bool initialize() override;
    uint8_t readRegister(uint8_t reg) override;
    bool readRegister(uint8_t reg, uint8_t *data, size_t size) override;
    void writeRegister(uint8_t reg, uint8_t value) override;

protected:
    TwoWire *wire = nullptr;
    uint8_t m_address = 0;
};

class IMU_Driver_SPI : public IMU_Driver
{
public:
    ~IMU_Driver_SPI() override = default;

    uint8_t readRegister(uint8_t reg) override;
    bool readRegister(uint8_t reg, uint8_t *data, size_t size) override;
    void writeRegister(uint8_t reg, uint8_t value) override;

protected:
    SPISettings _spiSettings;
    int8_t cs_pin = 0;
    int8_t int_pin = 0;
};
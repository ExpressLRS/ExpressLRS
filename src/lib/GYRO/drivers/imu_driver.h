#pragma once

#include "targets.h"
#include "SPI.h"

class IMU_Driver
{
    public:
        uint16_t gyroSampleRate = 1000;
        uint16_t period_us      = 1000;
        
        float   accScaleG, acc1G_adc, gyroScaleRad, gyroScaleDeg;

        virtual const char * GetMPUName();
        virtual bool initialize();
        virtual void start();

        virtual bool isDataReady();
        virtual bool rawRead(int16_t *ax, int16_t *ay, int16_t *az, int16_t *gx, int16_t *gy, int16_t *gz);

        virtual uint8_t readRegister(uint8_t reg);
        virtual bool readRegister(uint8_t reg, uint8_t *data, size_t size);
        virtual void writeRegister(uint8_t reg, uint8_t value);
        void writeRegisterBits(uint8_t registerID, uint8_t mask, uint8_t value);
        void setupInterrupt(uint8_t pin);
        bool interruptReceived();
        void setInterruptReceived(bool val);

        long non_ready_errors = 0;

};

class IMU_Driver_I2C : public IMU_Driver 
{
    public:
        uint8_t m_address;
    
        uint8_t readRegister(uint8_t reg);
        bool readRegister(uint8_t reg, uint8_t *data, size_t size);
        void writeRegister(uint8_t reg, uint8_t value);
};


class IMU_Driver_SPI : public IMU_Driver 
{
    public:
        SPISettings _spiSettings;
        uint8_t cs_pin = 0;
        uint8_t int_pin = 0;
    
        uint8_t readRegister(uint8_t reg);
        bool readRegister(uint8_t reg, uint8_t *data, size_t size);
        void writeRegister(uint8_t reg, uint8_t value);
        void setInterruptHandler(uint8_t pin);
};
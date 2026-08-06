#include "targets.h"

#if defined(GYRO_SUPPORT)
#include "mpu6050.h"
#include "mpu6050_regs.h"
//#include "MPU6050_6Axis_MotionApps612.h"
#include "logging.h"
#include "config.h"
#include <Wire.h>

#define I2C_MASTER_FREQ_HZ 400000

static uint8_t accScaleCode, gyroScaleCode;


const char * IMU_MPU6050::GetMPUName() {
    return "MPU6050";
}

static bool testConnection(IMU_Driver* mpu) {
    uint8_t id = 0;
    // Get Device ID
    bool ok = mpu->readRegister(MPU6050_RA_WHO_AM_I, &id, 1);
    id = (id >> 1) & 0x3F; // Bit 0 is reserved, starts at bit 1, 6 bits
    DBGLN("Id returned = 0x%x",id);

    // 0x34 is the ID expected for this chip, the other are known alernatives
    bool known_mpu6050_id = (id == 0x34 || id == 0x38 || id == 0x39);
    return ok && known_mpu6050_id;
}

bool IMU_MPU6050::initialize() {
    Wire.setTimeOut(5);
    m_address = MPU6050_DEFAULT_ADDRESS;     // Defaults is MPU6050_ADDRESS_AD0_LOW (0x68)
   
    DBGLN("Detecting MPU6050 (Address 0x68)");

    bool found = false;
    for (int8_t i=0;i<5;i++) {
        //if (mpu->testConnection()) 
        if (testConnection(this)) 
        {
            found = true;
            break;
        }
        delay(50);
    }

    if (!found) {
        //mpu = nullptr;
        DBGLN("Detecting MPU6050 (Alt Address 0x69)");
        m_address = 0x69;    // Use the alternate address (0x69)
        //mpu =  new MPU6050(m_address);
        //I2Cdev::readTimeout = 1; // 1ms timeout instead of 1000ms (1s)

        found = false;
        for (int8_t i=0;i<5;i++) {
            //if (mpu->testConnection()) 
            if (testConnection(this))
            {
                found = true;
                break;
            }
            delay(50);
        }
    }

    if (!found) 
    {
        DBGLN("MPU6050 not found!");
        return false;
    }

    gyroSampleRate = 1000;
    period_us = (1000000 / gyroSampleRate);

    accScaleCode = MPU6050_ACCEL_FS_16; // Acceleation 16G
    accScaleG  = 16 / 32768.0;         //   multiply adc by this to get Gs
    acc1G_adc = 32768 / 16;            //   1G in adc value

    gyroScaleCode = MPU6050_GYRO_FS_2000;  
    gyroScaleDeg = 2000.0 / 32768.0;             //   multiply adc by this to get deg°/s
    gyroScaleRad = radians(gyroScaleDeg);        //   multiply adc by this to get rad°/s  

    // Reset
    writeRegister(MPU6050_RA_PWR_MGMT_1, 1 << MPU6050_PWR1_DEVICE_RESET_BIT); // Reset Device
    vTaskDelay(50 * portTICK_PERIOD_MS);

    // Initialize Power Settings, no Sleep, Clock Selection XGyro
    uint8_t pwr =  (0 << MPU6050_PWR1_SLEEP_BIT) | // No SLeep
                   (MPU6050_CLOCK_PLL_XGYRO  << MPU6050_PWR1_CLKSEL_BIT); // Clock Selection XGyro
    writeRegister(MPU6050_RA_PWR_MGMT_1, pwr);

    // Accel Range, 16G
    // MPU6050_ACCEL_FS_16
    writeRegister(MPU6050_RA_ACCEL_CONFIG, accScaleCode << MPU6050_ACONFIG_AFS_SEL_BIT);

    // Gyro Range, 2000 deg/sec
    // MPU6050_GYRO_FS_2000
    writeRegister(MPU6050_RA_GYRO_CONFIG, gyroScaleCode << MPU6050_GCONFIG_FS_SEL_BIT); 
    
    // Interrupt Enabled
    writeRegister(MPU6050_RA_INT_ENABLE, 1 << MPU6050_INTERRUPT_DATA_RDY_BIT); // INT_ENABLE, DATA_RDY_EN

    // Interrupt Pin.. not using one right now
    //writeRegister(MPU6050_RA_INT_PIN_CFG, 0x02);  // INT_PIN_CFG, I2C_BYPASS_EN

    //Set frequency filters
    // MPU6050_DLPF_BW_188  = LPF (42Hz, 1khz sample rate, delay = 1.9ms)
    writeRegister(MPU6050_RA_CONFIG, MPU6050_DLPF_BW_42 << MPU6050_CFG_DLPF_CFG_BIT);

    return true;
}

void IMU_MPU6050::start() {
    DBGLN("MPU6050 Start");
}

bool IMU_MPU6050::isDataReady() 
{
    uint8_t status = 0;
    //bool readOk = I2Cdev::readByte(m_address, MPU6050_RA_INT_STATUS, &status, 0) == 1;
    bool readOk = readRegister(MPU6050_RA_INT_STATUS, &status, 1);
    // Check Data Ready Bit
    return readOk && ((status & (1<<MPU6050_INTERRUPT_DATA_RDY_BIT)) != 0);
}

bool IMU_MPU6050::rawRead(int16_t *ax, int16_t *ay, int16_t *az, int16_t *gx, int16_t *gy, int16_t *gz) 
{
    uint8_t buffer[14];

    //do the same, but retun error:  mpu->getMotion6(ax, ay, az, gx, gy, gz);
    //bool readOk = I2Cdev::readBytes(m_address, MPU6050_RA_ACCEL_XOUT_H, 14, buffer, 0) == 14;
    bool readOk = readRegister(MPU6050_RA_ACCEL_XOUT_H, buffer, 14);

    if (readOk) {
        *ax = (((int16_t)buffer[0]) << 8) | buffer[1];
        *ay = (((int16_t)buffer[2]) << 8) | buffer[3];
        *az = (((int16_t)buffer[4]) << 8) | buffer[5];
        *gx = (((int16_t)buffer[8]) << 8) | buffer[9];
        *gy = (((int16_t)buffer[10]) << 8) | buffer[11];
        *gz = (((int16_t)buffer[12]) << 8) | buffer[13];
    } else {
        *ax = *ay = *az = 0;
        *gx = *gy = *gz = 0;
    } 

    return readOk;
}

#endif

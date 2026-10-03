#include "targets.h"

#if defined(PLATFORM_ESP32)

#include "sc7u22.h"

#include <Arduino.h>
#include <Wire.h>

#include "logging.h"

const char *IMU_SC7U22::GetMPUName()
{
    return "SC7U22";
}

bool IMU_SC7U22::detect()
{
    const uint8_t addresses[] = {I2C_ADDRESS_SDO_HIGH, I2C_ADDRESS_SDO_LOW};

    for (const uint8_t address : addresses)
    {
        m_address = address;

        // Ensure the general register bank is selected before reading WHO_AM_I.
        writeRegister(REG_SEG_SEL, 0x00, 10);

        for (uint8_t attempt = 0; attempt < 5; ++attempt)
        {
            uint8_t chipId = 0;
            if (readRegister(REG_WHO_AM_I, &chipId, 1) && chipId == CHIP_ID)
            {
                DBGLN("SC7U22 found at I2C address 0x%x", m_address);
                return true;
            }
            delay(1);
        }
    }

    return false;
}

bool IMU_SC7U22::configure()
{
    writeRegister(REG_SEG_SEL, 0x00, 10);  // Segment Selection, normal Registers
    writeRegister(REG_COM_CONF, COM_CONF_BDU | COM_CONF_ADDR_AUTO, 10); // Mem-Reset, Block-Data-Update, Auto-Increment-addr

    // SC7U22 requires two soft reset writes for reliable startup.
    writeRegister(REG_SOFT_RST, SOFT_RESET_VALUE, 1);
    writeRegister(REG_SOFT_RST, SOFT_RESET_VALUE, 1);
    delay(RESET_DELAY_MS);

    writeRegister(REG_SEG_SEL, 0x00, 10);  // Segment Selection, normal Registers
    writeRegister(REG_COM_CONF, COM_CONF_BDU | COM_CONF_ADDR_AUTO, 10); // Block-Data-Update, Auto-Increment-addr

    writeRegister(REG_INT1_OUT_SEL1, INT1_OUT_SEL1_GYR);   // Triger Int1 line when Gyro data is ready

    writeRegister(REG_PWR_CTRL, 0x00, 1);               // Disable Temp, Gyro, ACC

    // Configure ACC
    writeRegister(REG_ACC_RANGE, ACC_RANGE_4G, 1);         // ACC range: 4g
    writeRegister(REG_ACC_CONF,
                  ACC_CONF_FILTER_PERF_HIGH | 
                  ACC_CONF_BWP_NORM_AVG4 | ACC_CONF_ODR_800, 1); // Acc: Filter:   ODR:800,
                  
    // Configure Gyro
    writeRegister(REG_GYR_RANGE, GYR_RANGE_2000DPS, 1);    // Gyro range: 2000dps
    writeRegister(REG_GYR_CONF,
                  GYR_CONF_FILTER_PERF_HIGH | GYR_CONF_NOISE_PERF_HIGH | // High performance, Noise Optimization on
                  GYR_CONF_BWP_NORM_AVG4 | GYR_CONF_ODR_800, 1); // Gyro: Filter:      ODR:800,
                  
    // Enable Gyro/Acc
    delay(5);
    writeRegister(REG_PWR_CTRL,
                  PWR_CTRL_TEMP_EN | PWR_CTRL_ACC_EN | PWR_CTRL_GYR_EN); // Enable Temp, Gyro and ACC

    delay(SENSOR_START_DELAY_MS); 

    return true;
}

bool IMU_SC7U22::initialize()
{
    IMU_Driver_I2C::initialize();
    wire->setClock(400000);
    wire->setTimeOut(2);

    DBGLN("Detecting SC7U22");

    if (!detect())
    {
        DBGLN("SC7U22 not found!");
        return false;
    }

    // Start at 800hz
    gyroSampleRate = 800;
    period_us = 1000000 / gyroSampleRate;

    accScaleG = 4.0f / 32768.0f;
    acc1G_adc = 32768.0f / 4.0f;
    gyroScaleDeg = 2000.0f / 32768.0f;
    gyroScaleRad = radians(gyroScaleDeg);

    configure();
    
    int16_t ax,ay,az,gx,gy,gz;
    int16_t ax1,ay1,az1;

    rawRead(&ax, &ay, &az, &gx, &gy, &gz);
    delay(10);
    rawRead(&ax1, &ay1, &az1, &gx, &gy, &gz);

    if ((ax==ax1) && (ay==ay1) && (az==az1)) 
    {
        DBGLN("SC7U22 failed to configure, trying again");
        configure();
    }


    DBGLN("SC7U22 initialized");
    return true;
}

void IMU_SC7U22::start()
{
    DBGLN("SC7U22 Start");
}

bool IMU_SC7U22::isDataReady()
{
    // The device produces data faster than the configured 800 Hz polling rate.
    // AHRS::tick() enforces period_us before calling this method.
    return true;
}

bool IMU_SC7U22::rawRead(int16_t *ax, int16_t *ay, int16_t *az,
                         int16_t *gx, int16_t *gy, int16_t *gz)
{
    uint8_t raw[12];
    if (!readRegister(REG_ACC_XH, raw, sizeof(raw)))
    {
        *ax = *ay = *az = 0;
        *gx = *gy = *gz = 0;
        return false;
    }

    // SC7U22 acceleration and gyro output registers are signed big-endian.
    *ax = static_cast<int16_t>((static_cast<uint16_t>(raw[0]) << 8) | raw[1]);
    *ay = static_cast<int16_t>((static_cast<uint16_t>(raw[2]) << 8) | raw[3]);
    *az = static_cast<int16_t>((static_cast<uint16_t>(raw[4]) << 8) | raw[5]);
    *gx = static_cast<int16_t>((static_cast<uint16_t>(raw[6]) << 8) | raw[7]);
    *gy = static_cast<int16_t>((static_cast<uint16_t>(raw[8]) << 8) | raw[9]);
    *gz = static_cast<int16_t>((static_cast<uint16_t>(raw[10]) << 8) | raw[11]);
    return true;
}

#endif

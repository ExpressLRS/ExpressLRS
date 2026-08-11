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

bool IMU_SC7U22::readRegisterRepeatedStart(uint8_t reg, uint8_t *data, size_t size)
{
    Wire.beginTransmission(m_address);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0)
    {
        return false;
    }

    if (Wire.requestFrom(m_address, size) != size)
    {
        return false;
    }

    return Wire.readBytes(data, size) == size;
}

void IMU_SC7U22::writeRegisterDelay(uint8_t reg, uint8_t value, uint16_t delayMs)
{
    writeRegister(reg, value);
    if (delayMs != 0)
    {
        delay(delayMs);
    }
}

bool IMU_SC7U22::detect()
{
    const uint8_t addresses[] = {I2C_ADDRESS_SDO_HIGH, I2C_ADDRESS_SDO_LOW};

    for (const uint8_t address : addresses)
    {
        m_address = address;

        // Ensure the general register bank is selected before reading WHO_AM_I.
        Wire.beginTransmission(m_address);
        Wire.write(REG_SEG_SEL);
        Wire.write(0x00);
        const uint8_t error = Wire.endTransmission();
        delay(1);
        if (error != 0)
        {
            continue;
        }

        for (uint8_t attempt = 0; attempt < 5; ++attempt)
        {
            uint8_t chipId = 0;
            if (readRegisterRepeatedStart(REG_WHO_AM_I, &chipId, 1) && chipId == CHIP_ID)
            {
                DBGLN("SC7U22 found at I2C address 0x%02x", m_address);
                return true;
            }
            delay(10);
        }
    }

    return false;
}

bool IMU_SC7U22::initialize()
{
    Wire.setTimeOut(5);
    DBGLN("Detecting SC7U22");

    if (!detect())
    {
        DBGLN("SC7U22 not found!");
        return false;
    }

    // The sensor runs at 1600 Hz, but polling at 800 Hz leaves enough time for
    // WiFi, PWM and the rest of the receiver processing.
    gyroSampleRate = 800;
    period_us = 1000000 / gyroSampleRate;

    accScaleG = 4.0f / 32768.0f;
    acc1G_adc = 32768.0f / 4.0f;
    gyroScaleDeg = 2000.0f / 32768.0f;
    gyroScaleRad = radians(gyroScaleDeg);

    writeRegisterDelay(REG_SEG_SEL, 0x00, 1);
    writeRegisterDelay(REG_COM_CONF, COM_CONF_BDU | COM_CONF_ADDR_AUTO, 1);

    // SC7U22 requires two reset writes for reliable startup.
    writeRegisterDelay(REG_SOFT_RST, SOFT_RESET_VALUE, 1);
    writeRegisterDelay(REG_SOFT_RST, SOFT_RESET_VALUE, RESET_DELAY_MS);

    writeRegisterDelay(REG_SEG_SEL, 0x00, 1);
    writeRegisterDelay(REG_COM_CONF, COM_CONF_BDU | COM_CONF_ADDR_AUTO, 1);
    writeRegisterDelay(REG_PWR_CTRL, 0x00, 1);
    writeRegisterDelay(REG_ACC_RANGE, ACC_RANGE_4G, 1);
    writeRegisterDelay(REG_GYR_RANGE, GYR_RANGE_2000DPS, 1);
    writeRegisterDelay(REG_ACC_CONF,
                       ACC_FILTER_PERF | ACC_BWP_OSR4_AVG1 | ACC_ODR_1600, 1);
    writeRegisterDelay(REG_GYR_CONF,
                       GYR_FILTER_PERF | GYR_BWP_OSR4_AVG1 | GYR_ODR_1600, 2);
    writeRegisterDelay(REG_PWR_CTRL,
                       PWR_CTRL_TEMP_EN | PWR_CTRL_ACC_EN | PWR_CTRL_GYR_EN,
                       SENSOR_START_DELAY_MS);

    DBGLN("SC7U22 initialized (1600Hz ODR, 800Hz polling)");
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
    if (!readRegisterRepeatedStart(REG_ACC_XH, raw, sizeof(raw)))
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

#pragma once

#include "stdint.h"
#include "imu_driver.h"

class IMU_SC7U22 : public IMU_Driver_I2C
{
    using IMU_Driver_I2C::IMU_Driver_I2C;

public:
    const char *GetMPUName() override;
    bool initialize() override;
    void start() override;

protected:
    bool isDataReady() override;
    bool rawRead(int16_t *ax, int16_t *ay, int16_t *az,
                 int16_t *gx, int16_t *gy, int16_t *gz) override;

private:
    static const uint8_t I2C_ADDRESS_SDO_LOW = 0x18;
    static const uint8_t I2C_ADDRESS_SDO_HIGH = 0x19;

    static const uint8_t REG_WHO_AM_I = 0x01;
    static const uint8_t REG_COM_CONF = 0x04;
    static const uint8_t REG_ACC_XH = 0x0C;
    static const uint8_t REG_ACC_CONF = 0x40;
    static const uint8_t REG_ACC_RANGE = 0x41;
    static const uint8_t REG_GYR_CONF = 0x42;
    static const uint8_t REG_GYR_RANGE = 0x43;
    static const uint8_t REG_SOFT_RST = 0x4A;
    static const uint8_t REG_PWR_CTRL = 0x7D;
    static const uint8_t REG_SEG_SEL = 0x7F;

    static const uint8_t COM_CONF_BDU = 0x40;
    static const uint8_t COM_CONF_ADDR_AUTO = 0x10;

    static const uint8_t PWR_CTRL_TEMP_EN = 0x08;
    static const uint8_t PWR_CTRL_ACC_EN = 0x04;
    static const uint8_t PWR_CTRL_GYR_EN = 0x02;

    static const uint8_t ACC_FILTER_PERF = 0x80;
    static const uint8_t ACC_BWP_OSR4_AVG1 = 0x00;
    static const uint8_t ACC_ODR_1600 = 0x0C;
    static const uint8_t ACC_RANGE_16G = 0x03;

    static const uint8_t GYR_FILTER_PERF = 0x80;
    static const uint8_t GYR_BWP_OSR4_AVG1 = 0x00;
    static const uint8_t GYR_ODR_1600 = 0x0C;
    static const uint8_t GYR_RANGE_2000DPS = 0x00;

    static const uint8_t CHIP_ID = 0x6A;
    static const uint8_t SOFT_RESET_VALUE = 0xA5;
    static const uint16_t RESET_DELAY_MS = 200;
    static const uint8_t SENSOR_START_DELAY_MS = 60;

    bool detect();
    bool readRegisterRepeatedStart(uint8_t reg, uint8_t *data, size_t size);
    void writeRegisterDelay(uint8_t reg, uint8_t value, uint16_t delayMs);
};

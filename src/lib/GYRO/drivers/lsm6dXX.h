#pragma once

#include "imu_driver.h"

// See INAV src/main/drivers/accgyro/accgyro_lms6dxx.c

class IMU_LSM6DXX_I2C : public IMU_Driver_I2C
{
    using IMU_Driver_I2C::IMU_Driver_I2C;

public:
    ~IMU_LSM6DXX_I2C() override = default;
    const char *GetMPUName() override;
    bool initialize() override;
    void start() override;

    bool isDataReady() override;
    bool rawRead(int16_t *ax, int16_t *ay, int16_t *az, int16_t *gx, int16_t *gy, int16_t *gz) override;
};

class IMU_LSM6DXX_SPI : public IMU_Driver_SPI
{
public:
    ~IMU_LSM6DXX_SPI() override = default;
    const char *GetMPUName() override;
    bool initialize() override;
    void start() override;

    bool isDataReady() override;
    bool rawRead(int16_t *ax, int16_t *ay, int16_t *az, int16_t *gx, int16_t *gy, int16_t *gz) override;
};

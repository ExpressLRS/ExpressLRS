#pragma once

#include "stdint.h"
#include "imu_driver.h"

// See INAV src/main/drivers/accgyro/accgyro_lms6dxx.c

class IMU_LSM6DXX_I2C : public IMU_Driver_I2C
{
    using IMU_Driver_I2C::IMU_Driver_I2C;
    public:
        const char *GetMPUName();
        bool initialize();
        void start();

    protected:
        bool isDataReady();
        bool rawRead(int16_t *ax, int16_t *ay, int16_t *az, int16_t *gx, int16_t *gy, int16_t *gz);
};


class IMU_LSM6DXX_SPI : public IMU_Driver_SPI
{
    using IMU_Driver_SPI::IMU_Driver_SPI;
    public:
        const char *GetMPUName();
        bool initialize();
        void start();

    protected:
        bool isDataReady();
        bool rawRead(int16_t *ax, int16_t *ay, int16_t *az, int16_t *gx, int16_t *gy, int16_t *gz);
};


#pragma once

#include "stdint.h"
#include "imu_driver.h"

// See betaflight src/main/drivers/accgyro/accgyro_mpu6050.c

class IMU_MPU6050 : public IMU_Driver_I2C
{
    using IMU_Driver_I2C::IMU_Driver_I2C;
    public:
        const char *GetMPUName();
        bool initialize();
        void start();

    protected:
        bool isDataReady();
        bool rawRead(int16_t *ax, int16_t *ay, int16_t *az, int16_t *gx, int16_t *gy, int16_t *gz);
    private:
};

#pragma once

#include "stdint.h"
#include "imu_driver.h"


class IMU_SC7U22_SPI : public IMU_Driver_SPI
{
    using IMU_Driver_SPI::IMU_Driver_SPI;

public:
    const char *GetMPUName() override;
    bool initialize() override;
    void start() override;

protected:
    bool isDataReady() override;
    bool rawRead(int16_t *ax, int16_t *ay, int16_t *az,
                 int16_t *gx, int16_t *gy, int16_t *gz) override;

private:
    bool detect();
    bool configure();
};




class IMU_SC7U22_I2C : public IMU_Driver_I2C
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
    bool detect();
    bool configure();
};

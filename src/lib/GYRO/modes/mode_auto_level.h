#pragma once
#include "gyro.h"
#include "mode_rate.h"

class LevelController : public RateController
{
public:
    void initialize(gyro_mode_t mode) override;
    void calculate_pid(float input_rpy[], float gyro_rpy[], float ang_rpy[]) override;
#if defined(DEBUG_LOG)
    void printState() override;
#endif

protected:
    rx_config_gyro_fmode_t fm_angle_settings {};
    PID pid_angle_roll, pid_angle_pitch;
};

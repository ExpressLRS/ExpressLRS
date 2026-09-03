#pragma once
#if defined(PLATFORM_ESP32)

#include "gyro.h"
#include "mode_rate.h"

class HoverController : public RateController
{
public:
    void initialize(gyro_mode_t mode) override;
    void calculate_pid(float input_rpy[], float gyro_rpy[], float ang_rpy[]) override;
#if defined(DEBUG_LOG)
    void printState() override;
#endif
protected:
    rx_config_gyro_fmode_t fm_angle_settings {};
    float hoverStrengthPitch = 0.0f;
    float hoverStrengthYaw = 0.0f;

    float errorPitch = 0.0f;
    float errorYaw = 0.0f;
};

#endif
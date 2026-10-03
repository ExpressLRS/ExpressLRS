#pragma once

#include "common.h"
#include "device.h"

enum eImuReadState : uint8_t
{
    irsNoImu,
    irsUninitialized,
    irsRunning
};

extern device_t Imu_device;

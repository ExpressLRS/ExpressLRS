#pragma once

#include "targets.h"

typedef enum
{
    GSENSOR_STATUS_FAIL = 0,
    GSENSOR_STATUS_NORMAL = 1,
    GSENSOR_STATUS_SUSPEND = 2
} Gsensor_Status_t;

typedef enum
{
    GSENSOR_SYSTEM_STATE_MOVING = 0,
    GSENSOR_SYSTEM_STATE_QUIET = 1
} Gsensor_System_State_t;

class Gsensor
{
private:
    int system_state;
    bool is_flipped;
public:
    bool init();
    void handle();
    bool hasTriggered(unsigned long now);
    void getGSensorData(float *X_DataOut, float *Y_DataOut, float *Z_DataOut);
    int getSystemState();
    bool isFlipped();
};

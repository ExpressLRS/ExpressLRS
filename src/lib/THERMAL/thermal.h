#pragma once

#include "targets.h"

typedef enum
{
    THERMAL_STATUS_FAIL = 0,
    THERMAL_STATUS_NORMAL = 1
} Thermal_Status_t;

class Thermal
{
private:
    uint8_t temp_value;

public:
    void init();
    void handle();
    uint8_t read_temp();
    void update_threshold(int index);
    uint8_t getTempValue() { return temp_value; }
};

#define THERMAL_FAN_DEFAULT_LOW_THRESHOLD   35
#define THERMAL_FAN_DEFAULT_HIGH_THRESHOLD  50

#define THERMAL_FAN_ALWAYS_ON_LOW_THRESHOLD     0xFF
#define THERMAL_FAN_ALWAYS_ON_HIGH_THRESHOLD    0xFD

#define THERMAL_FAN_OFF_LOW_THRESHOLD     0x7D
#define THERMAL_FAN_OFF_HIGH_THRESHOLD    0x7F

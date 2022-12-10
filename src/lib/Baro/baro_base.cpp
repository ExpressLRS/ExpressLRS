#include <math.h>

#include "baro_base.h"

/**
 * @brief: Return altitude in cm from pressure in deci-Pascals
 **/
int32_t BaroBase::pressureToAltitude(uint32_t pressuredPa)
{
    const float seaLeveldPa = 1013250; // 1013.25hPa
    return 4433000 * (1.0 - pow(pressuredPa / seaLeveldPa, 0.1903));
}


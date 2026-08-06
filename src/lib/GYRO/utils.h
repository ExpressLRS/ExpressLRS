#pragma once
#include "gyro_types.h"

float us_command_to_float(uint16_t us);
float crsf_command_to_float(uint32_t us);
float us_command_to_float(uint8_t ch, uint16_t us);
uint16_t float_to_us(uint8_t ch, float value);


// Channel configuration for minimum, subtrim and maximum us values
//extern uint16_t ch_us[GYRO_MAX_CHANNELS][3];

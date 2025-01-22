#pragma once

#include "targets.h"
#include "device.h"

enum adc_reading {
    ADC_JOYSTICK,
    ADC_MAX_DEVICES
};

extern int getADCReading(adc_reading reading);
extern device_t ADC_device;

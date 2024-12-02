#pragma once

#include "device.h"

#if defined(GPIO_PIN_BUZZER)
extern device_t Buzzer_device;
#define HAS_BUZZER
#endif

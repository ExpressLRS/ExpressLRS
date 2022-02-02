#pragma once

#include "device.h"

#if defined(GPIO_PIN_BUZZER) && (GPIO_PIN_BUZZER != UNDEF_PIN)
extern device_t Buzzer_device;
#define HAS_BUZZER
#endif

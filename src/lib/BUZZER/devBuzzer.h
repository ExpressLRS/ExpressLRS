#pragma once

#include "device.h"

#if defined(GPIO_PIN_BUZZER) && (GPIO_PIN_BUZZER != UNDEF_PIN) && !defined(DISABLE_ALL_BEEPS)
extern device_t Buzzer_device;
#define HAS_BUZZER
#endif

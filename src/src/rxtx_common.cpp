#include "targets.h"

#if defined(USE_I2C)
#include <Wire.h>
#endif

void setupTargetCommon()
{
#if defined(USE_I2C)
  Wire.begin(GPIO_PIN_SDA, GPIO_PIN_SCL);
#endif
}

#include "targets.h"
#include "device.h"
#include "logging.h"

#if defined(GPIO_PIN_PA_PDET) && GPIO_PIN_PA_PDET != UNDEF_PIN
static int start()
{
    analogSetPinAttenuation(GPIO_PIN_PA_PDET, ADC_0db);
    return DURATION_IMMEDIATELY;
}

static int timeout()
{
    INFOLN("PDET = %dmV", analogReadMilliVolts(GPIO_PIN_PA_PDET));
    return 1000;
}

device_t PDET_device = {
    .initialize = NULL,
    .start = start,
    .event = NULL,
    .timeout = timeout
};
#endif
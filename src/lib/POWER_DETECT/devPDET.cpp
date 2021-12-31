#include "targets.h"
#include "device.h"
#include "logging.h"
#include "POWERMGNT.h"

#if defined(GPIO_PIN_PA_PDET) && GPIO_PIN_PA_PDET != UNDEF_PIN

#if defined(USE_SKY85321)
#define SKY85321_MAX_DBM_INPUT 5
#endif

#define PDET_HYSTERESIS        0.7
#define PDET_SAMPLE_PERIOD     1000
#define PDET_BUSY_PERIOD       999 // 999 to shift the next measurement time into a transmission period.
#define PDET_SETTLE_PERIOD     5000

extern bool busyTransmitting;

static int start()
{
    analogSetPinAttenuation(GPIO_PIN_PA_PDET, ADC_0db);
    return PDET_SETTLE_PERIOD;
}

static int timeout()
{
    if (!busyTransmitting) return PDET_BUSY_PERIOD;

    uint16_t Pdet = analogReadMilliVolts(GPIO_PIN_PA_PDET);

    if (!busyTransmitting) return PDET_BUSY_PERIOD; // Check transmission did not stop during Pdet measurement.

    float dBm = SKY85321_PDET_SLOPE * Pdet + SKY85321_PDET_INTERCEPT;
    
    INFOLN("Pdet = %d mV", Pdet);
    // INFOLN("%d dBm", dBm); // how do we print floats? :|
    LOGGING_UART.print(dBm, 2);
    LOGGING_UART.println(" dBm");

    if (dBm < ((float)(POWERMGNT::getPowerIndBm()) - PDET_HYSTERESIS) && POWERMGNT::currentSX1280Ouput() < SKY85321_MAX_DBM_INPUT)
    {
        POWERMGNT::incSX1280Ouput();
        return PDET_SETTLE_PERIOD; // Settle period
    }
    else if (dBm > (POWERMGNT::getPowerIndBm() + PDET_HYSTERESIS))
    {
        POWERMGNT::decSX1280Ouput();
        return PDET_SETTLE_PERIOD;
    }

    return PDET_SAMPLE_PERIOD;
}

device_t PDET_device = {
    .initialize = NULL,
    .start = start,
    .event = NULL,
    .timeout = timeout
};
#endif
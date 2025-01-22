#include "targets.h"
#if defined(TARGET_TX)
#include "devADC.h"

#define ADC_READING_PERIOD_MS 20

static volatile int analogReadings[ADC_MAX_DEVICES];

int getADCReading(adc_reading reading)
{
    return analogReadings[reading];
}

static int start()
{
#if defined(GPIO_PIN_JOYSTICK)
    if (GPIO_PIN_JOYSTICK != UNDEF_PIN)
    {
        return DURATION_IMMEDIATELY;
    }
#endif
    return DURATION_NEVER;
}

static int timeout()
{
    extern volatile bool busyTransmitting;
    static bool fullWait = true;

    // if called because of a full-timeout and the main loop is NOT transmitting then we will
    // leave the fullWait flag true and return with an immediate timeout so we can wait for
    // the main loop to be transmitting, which will pop us into the next state.
    if (fullWait && !busyTransmitting) return DURATION_IMMEDIATELY;
    fullWait = false;

    // If the main loop is transmitting then return with an immediate timeout until it transitions
    // to not transmitting
    if (busyTransmitting) return DURATION_IMMEDIATELY;

    // If we reach this point we are assured that the main loop has just transitioned from
    // transmitting to not transmitting, so it's safe to read the ADC
#if defined(GPIO_PIN_JOYSTICK)
    if (GPIO_PIN_JOYSTICK != UNDEF_PIN)
    {
        analogReadings[ADC_JOYSTICK] = analogRead(GPIO_PIN_JOYSTICK);
    }
#endif
    fullWait = true;
    return ADC_READING_PERIOD_MS;
}

device_t ADC_device = {
    .initialize = nullptr,
    .start = start,
    .event = nullptr,
    .timeout = timeout,
};
#endif

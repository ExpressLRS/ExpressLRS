#include "targets.h"
#include "devADC.h"

#define ADC_READING_PERIOD_MS 20

static volatile int analogReadings[ADC_MAX_DEVICES];

int getADCReading(adc_reading reading)
{
    return analogReadings[reading];
}

static int start()
{
    if (GPIO_PIN_JOYSTICK != UNDEF_PIN)
    {
        return DURATION_IMMEDIATELY;
    }
    return DURATION_NEVER;
}

static int timeout()
{
    extern volatile bool busyTransmitting;
    if (busyTransmitting)
    {
        return DURATION_IMMEDIATELY;
    }
    if (GPIO_PIN_JOYSTICK != UNDEF_PIN)
    {
        analogReadings[ADC_JOYSTICK] = analogRead(GPIO_PIN_JOYSTICK);
    }
    return ADC_READING_PERIOD_MS;
}

device_t ADC_device = {
    .initialize = nullptr,
    .start = start,
    .event = nullptr,
    .timeout = timeout,
};

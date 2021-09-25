#ifdef TARGET_TX

#include "targets.h"
#include "common.h"
#include "device.h"

#include "crsf_protocol.h"
#include "POWERMGNT.h"

static bool startLED = false;
constexpr uint8_t LEDSEQ_RADIO_FAILED[] = {  20, 100 }; // 200ms on, 1000ms off 

static uint16_t flashLED(uint8_t pin, uint8_t inverted_pin, const uint8_t durations[], uint8_t durationCounts)
{
    static int counter = 0;

    if(counter % 2 == 0)
        digitalWrite(pin, LOW ^ inverted_pin);
    else
        digitalWrite(pin, HIGH ^ inverted_pin);
    if (counter >= durationCounts)
    {
        counter = 0;
    }
    return durations[counter++] * 10;
}

static void initialize()
{
    #if defined(GPIO_PIN_LED_GREEN) && (GPIO_PIN_LED_GREEN != UNDEF_PIN)
        pinMode(GPIO_PIN_LED_GREEN, OUTPUT);
        digitalWrite(GPIO_PIN_LED_GREEN, HIGH ^ GPIO_LED_GREEN_INVERTED);
    #endif // GPIO_PIN_LED_GREEN
    #if defined(GPIO_PIN_LED_RED) && (GPIO_PIN_LED_RED != UNDEF_PIN)
        pinMode(GPIO_PIN_LED_RED, OUTPUT);
        digitalWrite(GPIO_PIN_LED_RED, LOW ^ GPIO_LED_RED_INVERTED);
    #endif // GPIO_PIN_LED_RED
    #if defined(TARGET_TX_FM30)
        pinMode(GPIO_PIN_LED_RED_GREEN, OUTPUT); // Green LED on "Red" LED (off)
        digitalWrite(GPIO_PIN_LED_RED_GREEN, HIGH);
        pinMode(GPIO_PIN_LED_GREEN_RED, OUTPUT); // Red LED on "Green" LED (off)
        digitalWrite(GPIO_PIN_LED_GREEN_RED, HIGH);
    #endif
}

static int start()
{
    return DURATION_NEVER;
}

static int timeout(std::function<void ()> sendSpam)
{
    #if defined(GPIO_PIN_LED_RED) && (GPIO_PIN_LED_RED != UNDEF_PIN)
        return flashLED(GPIO_PIN_LED_RED, GPIO_LED_RED_INVERTED, LEDSEQ_RADIO_FAILED, sizeof(LEDSEQ_RADIO_FAILED));
    #else
        return DURATION_NEVER;
    #endif
}

static int event(std::function<void ()> sendSpam)
{
    switch (connectionState)
    {
    case connected:
        #if defined(GPIO_PIN_LED_RED) && (GPIO_PIN_LED_RED != UNDEF_PIN)
            digitalWrite(GPIO_PIN_LED_RED, HIGH ^ GPIO_LED_RED_INVERTED);
        #endif // GPIO_PIN_LED_RED
        return DURATION_NEVER;
    case disconnected:
        #if defined(GPIO_PIN_LED_RED) && (GPIO_PIN_LED_RED != UNDEF_PIN)
            digitalWrite(GPIO_PIN_LED_RED, LOW ^ GPIO_LED_RED_INVERTED);
        #endif // GPIO_PIN_LED_RED
        return DURATION_NEVER;
    case radioFailed:
        #if defined(GPIO_PIN_LED_GREEN) && (GPIO_PIN_LED_GREEN != UNDEF_PIN)
            digitalWrite(GPIO_PIN_LED_GREEN, LOW ^ GPIO_LED_GREEN_INVERTED);
        #endif // GPIO_PIN_LED_GREEN
        startLED = true;
        return timeout(sendSpam);
    default:
        return DURATION_NEVER;
    }
}

device_t LED_device = {
    .initialize = initialize,
    .start = start,
    .event = event,
    .timeout = timeout
};

#endif // TARGET_TX
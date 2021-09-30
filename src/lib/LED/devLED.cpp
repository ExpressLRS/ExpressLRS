#include "targets.h"
#include "common.h"
#include "device.h"

#include "crsf_protocol.h"
#include "POWERMGNT.h"

static bool startLED = false;
extern bool InBindingMode;

constexpr uint8_t LEDSEQ_RADIO_FAILED[] = { 20, 100 }; // 200ms on, 1000ms off 
constexpr uint8_t LEDSEQ_DISCONNECTED[] = { 50, 50 };  // 500ms on, 500s off 
constexpr uint8_t LEDSEQ_WIFI_UPDATE[] = { 2, 3 };     // 20ms on, 30s off 
constexpr uint8_t LEDSEQ_BINDING[] = { 10, 10, 10, 100 };   // 100ms on, 100ms off, 100ms on, 1s off

#if defined(GPIO_PIN_LED_GREEN) || defined(GPIO_PIN_LED)
static uint8_t _pin = -1;
static uint8_t _pin_inverted;
static const uint8_t *_durations;
static uint8_t _count;
static uint8_t _counter = 0;

static uint16_t updateLED()
{
    if(_counter % 2 == 0)
        digitalWrite(_pin, LOW ^ _pin_inverted);
    else
        digitalWrite(_pin, HIGH ^ _pin_inverted);
    if (_counter >= _count)
    {
        _counter = 0;
    }
    return _durations[_counter++] * 10;
}

static uint16_t flashLED(uint8_t pin, uint8_t pin_inverted, const uint8_t durations[], uint8_t count)
{
    _counter = 0;
    _pin = pin;
    _pin_inverted = pin_inverted;
    _durations = durations;
    _count = count;
    return updateLED();
}
#endif

static void initialize()
{
    #if defined(TARGET_TX)
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
    #endif
    #if defined(TARGET_RX)
        #ifdef GPIO_PIN_LED_GREEN
            pinMode(GPIO_PIN_LED_GREEN, OUTPUT);
            digitalWrite(GPIO_PIN_LED_GREEN, LOW ^ GPIO_LED_GREEN_INVERTED);
        #endif /* GPIO_PIN_LED_GREEN */
        #ifdef GPIO_PIN_LED_RED
            pinMode(GPIO_PIN_LED_RED, OUTPUT);
            digitalWrite(GPIO_PIN_LED_RED, LOW ^ GPIO_LED_RED_INVERTED);
        #endif /* GPIO_PIN_LED_RED */
        #if defined(GPIO_PIN_LED)
            pinMode(GPIO_PIN_LED, OUTPUT);
            digitalWrite(GPIO_PIN_LED, LOW ^ GPIO_LED_RED_INVERTED);
        #endif /* GPIO_PIN_LED */
    #endif
}

static int timeout(std::function<void ()> sendSpam)
{
#if defined(GPIO_PIN_LED_GREEN) || defined(GPIO_PIN_LED)
    if (_pin == -1)
    {
        return DURATION_NEVER;
    }
    return updateLED();
#else
    return DURATION_NEVER;
#endif
}

static int event(std::function<void ()> sendSpam)
{
    #if defined(TARGET_RX) && defined(GPIO_PIN_LED)
        if (InBindingMode)
        {
            return flashLED(GPIO_PIN_LED, GPIO_LED_RED_INVERTED, LEDSEQ_BINDING, sizeof(LEDSEQ_BINDING));
        }
    #endif
    switch (connectionState)
    {
    case connected:
        #if defined(TARGET_TX) && defined(GPIO_PIN_LED_RED) && (GPIO_PIN_LED_RED != UNDEF_PIN)
            digitalWrite(GPIO_PIN_LED_RED, HIGH ^ GPIO_LED_RED_INVERTED);
        #endif // GPIO_PIN_LED_RED
        #if defined(TARGET_RX)
            #ifdef GPIO_PIN_LED_GREEN
                digitalWrite(GPIO_PIN_LED_GREEN, HIGH ^ GPIO_LED_GREEN_INVERTED);
            #endif

            #ifdef GPIO_PIN_LED_RED
                digitalWrite(GPIO_PIN_LED_RED, HIGH ^ GPIO_LED_RED_INVERTED);
            #endif

            #ifdef GPIO_PIN_LED
                digitalWrite(GPIO_PIN_LED, HIGH ^ GPIO_LED_RED_INVERTED); // turn on led
            #endif
        #endif
        return DURATION_NEVER;
    case disconnected:
        #if defined(TARGET_TX)
            #if defined(GPIO_PIN_LED_RED) && (GPIO_PIN_LED_RED != UNDEF_PIN)
                digitalWrite(GPIO_PIN_LED_RED, LOW ^ GPIO_LED_RED_INVERTED);
            #endif
            return DURATION_NEVER;
        #endif
        #if defined(TARGET_RX)
            #ifdef GPIO_PIN_LED_GREEN
                digitalWrite(GPIO_PIN_LED_GREEN, LOW ^ GPIO_LED_GREEN_INVERTED);
            #endif
            #ifdef GPIO_PIN_LED_RED
                digitalWrite(GPIO_PIN_LED_RED, LOW ^ GPIO_LED_RED_INVERTED);
            #endif
            #ifdef GPIO_PIN_LED
                digitalWrite(GPIO_PIN_LED, LOW ^ GPIO_LED_RED_INVERTED); // turn off led
            #endif
            #ifdef GPIO_PIN_LED
                return flashLED(GPIO_PIN_LED, GPIO_LED_RED_INVERTED, LEDSEQ_DISCONNECTED, sizeof(LEDSEQ_DISCONNECTED));
            #elif GPIO_PIN_LED_GREEN
                return flashLED(GPIO_PIN_LED_GREEN, GPIO_LED_GREEN_INVERTED, LEDSEQ_DISCONNECTED, sizeof(LEDSEQ_DISCONNECTED));
            #endif
        #endif
    case wifiUpdate:
        #if defined(TARGET_TX)
            return DURATION_NEVER;
        #endif
        #if defined(TARGET_RX) && defined(GPIO_PIN_LED)
            return flashLED(GPIO_PIN_LED, GPIO_LED_RED_INVERTED, LEDSEQ_WIFI_UPDATE, sizeof(LEDSEQ_WIFI_UPDATE));
        #else
            return DURATION_NEVER;
        #endif
    case radioFailed:
        #if defined(TARGET_TX) && defined(GPIO_PIN_LED_GREEN) && (GPIO_PIN_LED_GREEN != UNDEF_PIN)
            digitalWrite(GPIO_PIN_LED_GREEN, LOW ^ GPIO_LED_GREEN_INVERTED);
        #endif // GPIO_PIN_LED_GREEN

        startLED = true;
        return timeout(sendSpam);
    default:
        return DURATION_NEVER;
    }
}

static int start()
{
    return event([](){});
}

device_t LED_device = {
    .initialize = initialize,
    .start = start,
    .event = event,
    .timeout = timeout
};

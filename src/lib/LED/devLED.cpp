#include "targets.h"
#include "common.h"
#include "devLED.h"

#if defined(TARGET_TX)
#include "POWERMGNT.h"
#endif

constexpr uint8_t LEDSEQ_RADIO_FAILED[] = { 20, 100 }; // 200ms on, 1000ms off
constexpr uint8_t LEDSEQ_DISCONNECTED[] = { 50, 50 };  // 500ms on, 500ms off
constexpr uint8_t LEDSEQ_WIFI_UPDATE[] = { 2, 3 };     // 20ms on, 30ms off
constexpr uint8_t LEDSEQ_BINDING[] = { 10, 10, 10, 100 };   // 2x 100ms blink, 1s pause
constexpr uint8_t LEDSEQ_MODEL_MISMATCH[] = { 10, 10, 10, 10, 10, 100 };   // 3x 100ms blink, 1s pause
constexpr uint8_t LEDSEQ_UPDATE[] = { 20, 5, 5, 5, 5, 40 };   // 200ms on, 2x 50ms off/on, 400ms off

static int8_t _pin = -1;
static uint8_t _pin_inverted;
static const uint8_t *_durations;
static uint8_t _count;
static uint8_t _counter = 0;
static bool hasRGBLeds = false;
static bool hasGBLeds = false;

static uint16_t updateLED()
{
    if (_pin == UNDEF_PIN)
    {
        return DURATION_NEVER;
    }
    if(_counter % 2 == 1)
        digitalWrite(_pin, LOW ^ _pin_inverted);
    else
        digitalWrite(_pin, HIGH ^ _pin_inverted);
    if (_counter >= _count)
    {
        _counter = 0;
    }
    return _durations[_counter++] * 10;
}

static uint16_t flashLED(int8_t pin, uint8_t pin_inverted, const uint8_t durations[], uint8_t count)
{
    _counter = 0;
    _pin = pin;
    _pin_inverted = pin_inverted;
    _durations = durations;
    _count = count;
    return updateLED();
}

static void initialize()
{
    if (GPIO_PIN_LED_BLUE != UNDEF_PIN)
    {
        pinMode(GPIO_PIN_LED_BLUE, OUTPUT);
        digitalWrite(GPIO_PIN_LED_BLUE, LOW ^ GPIO_LED_BLUE_INVERTED);
    }
    if (GPIO_PIN_LED_GREEN != UNDEF_PIN)
    {
        pinMode(GPIO_PIN_LED_GREEN, OUTPUT);
        digitalWrite(GPIO_PIN_LED_GREEN, HIGH ^ GPIO_LED_GREEN_INVERTED);
    }
    if (GPIO_PIN_LED_RED != UNDEF_PIN)
    {
        pinMode(GPIO_PIN_LED_RED, OUTPUT);
        digitalWrite(GPIO_PIN_LED_RED, LOW ^ GPIO_LED_RED_INVERTED);
    }
    if (GPIO_PIN_LED_BLUE != UNDEF_PIN && GPIO_PIN_LED_GREEN != UNDEF_PIN && GPIO_PIN_LED_RED != UNDEF_PIN)
    {
        hasRGBLeds = true;
        digitalWrite(GPIO_PIN_LED_GREEN, LOW);
        digitalWrite(GPIO_PIN_LED_RED, HIGH);
        digitalWrite(GPIO_PIN_LED_BLUE, LOW);
    }
    else if (GPIO_PIN_LED_BLUE != UNDEF_PIN && GPIO_PIN_LED_GREEN != UNDEF_PIN && GPIO_PIN_LED_RED == UNDEF_PIN)
    {
        hasGBLeds = true;
    }
}

static int timeout()
{
    return updateLED();
}

#if defined(TARGET_TX)
static void setPowerLEDs()
{
    if (hasGBLeds)
    {
        switch (POWERMGNT::currPower())
        {
        case PWR_250mW:
            digitalWrite(GPIO_PIN_LED_BLUE, HIGH);
            digitalWrite(GPIO_PIN_LED_GREEN, HIGH);
            break;
        case PWR_500mW:
            digitalWrite(GPIO_PIN_LED_BLUE, LOW);
            digitalWrite(GPIO_PIN_LED_GREEN, HIGH);
            break;
        case PWR_10mW:
        case PWR_25mW:
        case PWR_50mW:
        case PWR_100mW:
        default:
            digitalWrite(GPIO_PIN_LED_BLUE, HIGH);
            digitalWrite(GPIO_PIN_LED_GREEN, LOW);
            break;
        }
    }
}
#endif

static int event()
{
    #if defined(TARGET_TX)
        setPowerLEDs();
    #else
        if (InBindingMode && GPIO_PIN_LED_RED != UNDEF_PIN)
        {
            return flashLED(GPIO_PIN_LED_RED, GPIO_LED_RED_INVERTED, LEDSEQ_BINDING, sizeof(LEDSEQ_BINDING));
        }
    #endif
    switch (connectionState)
    {
    case connected:
        if (hasRGBLeds)
        {
            digitalWrite(GPIO_PIN_LED_GREEN, HIGH);
            digitalWrite(GPIO_PIN_LED_RED, LOW);
            digitalWrite(GPIO_PIN_LED_BLUE, LOW);
        }
        else if (GPIO_PIN_LED_GREEN != UNDEF_PIN)
        {
            digitalWrite(GPIO_PIN_LED_GREEN, HIGH ^ GPIO_LED_GREEN_INVERTED);
        }
        if (GPIO_PIN_LED_RED != UNDEF_PIN)
        {
            if (!connectionHasModelMatch || !teamraceHasModelMatch)
            {
                return flashLED(GPIO_PIN_LED_RED, GPIO_LED_RED_INVERTED, LEDSEQ_MODEL_MISMATCH, sizeof(LEDSEQ_MODEL_MISMATCH));
            }
            else if (!hasRGBLeds)
            {
                digitalWrite(GPIO_PIN_LED_RED, HIGH ^ GPIO_LED_RED_INVERTED); // turn on led
            }
        }
        return DURATION_NEVER;
    case disconnected:
        if (hasRGBLeds)
        {
            digitalWrite(GPIO_PIN_LED_GREEN, LOW);
            digitalWrite(GPIO_PIN_LED_BLUE, LOW);
            return flashLED(GPIO_PIN_LED_RED, GPIO_LED_RED_INVERTED, LEDSEQ_DISCONNECTED, sizeof(LEDSEQ_DISCONNECTED));
        }
        else if (GPIO_PIN_LED_GREEN != UNDEF_PIN && GPIO_PIN_LED_RED != UNDEF_PIN)
        {
            digitalWrite(GPIO_PIN_LED_GREEN, LOW ^ GPIO_LED_GREEN_INVERTED);
            digitalWrite(GPIO_PIN_LED_RED, LOW ^ GPIO_LED_RED_INVERTED);
        }
        else if (GPIO_PIN_LED_RED != UNDEF_PIN)
        {
            return flashLED(GPIO_PIN_LED_RED, GPIO_LED_RED_INVERTED, LEDSEQ_DISCONNECTED, sizeof(LEDSEQ_DISCONNECTED));
        }
        else if (GPIO_PIN_LED_GREEN != UNDEF_PIN)
        {
            return flashLED(GPIO_PIN_LED_GREEN, GPIO_LED_GREEN_INVERTED, LEDSEQ_DISCONNECTED, sizeof(LEDSEQ_DISCONNECTED));
        }
        return DURATION_NEVER;
    case wifiUpdate:
        if (hasRGBLeds)
        {
            digitalWrite(GPIO_PIN_LED_GREEN, LOW);
            digitalWrite(GPIO_PIN_LED_RED, LOW);
            return flashLED(GPIO_PIN_LED_BLUE, GPIO_LED_BLUE_INVERTED, LEDSEQ_WIFI_UPDATE, sizeof(LEDSEQ_WIFI_UPDATE));
        }
        else if (GPIO_PIN_LED_RED != UNDEF_PIN)
        {
            return flashLED(GPIO_PIN_LED_RED, GPIO_LED_RED_INVERTED, LEDSEQ_WIFI_UPDATE, sizeof(LEDSEQ_WIFI_UPDATE));
        }
        return DURATION_NEVER;
    case radioFailed:
        if (hasRGBLeds)
        {
            digitalWrite(GPIO_PIN_LED_GREEN, LOW);
            digitalWrite(GPIO_PIN_LED_BLUE, LOW);
            return flashLED(GPIO_PIN_LED_RED, GPIO_LED_RED_INVERTED, LEDSEQ_RADIO_FAILED, sizeof(LEDSEQ_RADIO_FAILED));
        }
        else if (GPIO_PIN_LED_GREEN != UNDEF_PIN)
        {
            digitalWrite(GPIO_PIN_LED_GREEN, LOW ^ GPIO_LED_GREEN_INVERTED);
        }
        if (GPIO_PIN_LED_RED != UNDEF_PIN)
        {
            return flashLED(GPIO_PIN_LED_RED, GPIO_LED_RED_INVERTED, LEDSEQ_RADIO_FAILED, sizeof(LEDSEQ_RADIO_FAILED));
        }
        return DURATION_NEVER;
    case noCrossfire:
        if (GPIO_PIN_LED_RED != UNDEF_PIN)
        {
            // technically nocrossfire is {10,100} but {20,100} is close enough
            return flashLED(GPIO_PIN_LED_RED, GPIO_LED_RED_INVERTED, LEDSEQ_RADIO_FAILED, sizeof(LEDSEQ_RADIO_FAILED));
        }
    case serialUpdate:
        if (GPIO_PIN_LED_RED != UNDEF_PIN)
        {
            return flashLED(GPIO_PIN_LED_RED, GPIO_LED_RED_INVERTED, LEDSEQ_UPDATE, sizeof(LEDSEQ_UPDATE));
        }
    default:
        return DURATION_NEVER;
    }
}

device_t LED_device = {
    .initialize = initialize,
    .start = event,
    .event = event,
    .timeout = timeout
};

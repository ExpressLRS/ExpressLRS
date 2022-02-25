#include "targets.h"
#include "common.h"
#include "device.h"

#include "crsf_protocol.h"
#include "POWERMGNT.h"

extern bool InBindingMode;
#if defined(TARGET_RX)
extern bool connectionHasModelMatch;
#endif

constexpr uint8_t LEDSEQ_RADIO_FAILED[] = { 20, 100 }; // 200ms on, 1000ms off
constexpr uint8_t LEDSEQ_DISCONNECTED[] = { 50, 50 };  // 500ms on, 500ms off
constexpr uint8_t LEDSEQ_WIFI_UPDATE[] = { 2, 3 };     // 20ms on, 30ms off
constexpr uint8_t LEDSEQ_BINDING[] = { 10, 10, 10, 100 };   // 2x 100ms blink, 1s pause
constexpr uint8_t LEDSEQ_MODEL_MISMATCH[] = { 10, 10, 10, 10, 10, 100 };   // 3x 100ms blink, 1s pause

static uint8_t _pin = -1;
static uint8_t _pin_inverted;
static const uint8_t *_durations;
static uint8_t _count;
static uint8_t _counter = 0;

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

static uint16_t flashLED(uint8_t pin, uint8_t pin_inverted, const uint8_t durations[], uint8_t count)
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
    // TODO for future PR, remove TARGET_TX, TARGET_RX, and TARGET_TX_FM30 defines.
    #if defined(TARGET_TX)
        #if defined(GPIO_PIN_LED_BLUE) && (GPIO_PIN_LED_BLUE != UNDEF_PIN)
            pinMode(GPIO_PIN_LED_BLUE, OUTPUT);
            digitalWrite(GPIO_PIN_LED_BLUE, LOW ^ GPIO_LED_BLUE_INVERTED);
        #endif // GPIO_PIN_BLUE_GREEN
        #if defined(GPIO_PIN_LED_GREEN) && (GPIO_PIN_LED_GREEN != UNDEF_PIN)
            pinMode(GPIO_PIN_LED_GREEN, OUTPUT);
            digitalWrite(GPIO_PIN_LED_GREEN, HIGH ^ GPIO_LED_GREEN_INVERTED);
        #endif // GPIO_PIN_LED_GREEN
        #if defined(GPIO_PIN_LED_RED) && (GPIO_PIN_LED_RED != UNDEF_PIN)
            pinMode(GPIO_PIN_LED_RED, OUTPUT);
            digitalWrite(GPIO_PIN_LED_RED, LOW ^ GPIO_LED_RED_INVERTED);
        #endif // GPIO_PIN_LED_RED
        #if defined(TARGET_TX_IFLIGHT)
            digitalWrite(GPIO_PIN_LED_GREEN, LOW);
            digitalWrite(GPIO_PIN_LED_RED, HIGH);
            digitalWrite(GPIO_PIN_LED_BLUE, LOW);
        #endif
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

static int timeout()
{
    return updateLED();
}

static void setPowerLEDs()
{
    #if defined(TARGET_TX_BETAFPV_2400_V1) || defined(TARGET_TX_BETAFPV_900_V1)
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
    #endif
}

static int event()
{
    #if defined(TARGET_RX) && defined(GPIO_PIN_LED)
        if (InBindingMode)
        {
            return flashLED(GPIO_PIN_LED, GPIO_LED_RED_INVERTED, LEDSEQ_BINDING, sizeof(LEDSEQ_BINDING));
        }
    #endif
    setPowerLEDs();
    switch (connectionState)
    {
    case connected:
        #if defined(TARGET_TX)
            #if defined(TARGET_TX_IFLIGHT)
                digitalWrite(GPIO_PIN_LED_GREEN, HIGH);
                digitalWrite(GPIO_PIN_LED_RED, LOW);
                digitalWrite(GPIO_PIN_LED_BLUE, LOW);
            #elif defined(GPIO_PIN_LED_RED) && (GPIO_PIN_LED_RED != UNDEF_PIN)
                digitalWrite(GPIO_PIN_LED_RED, HIGH ^ GPIO_LED_RED_INVERTED);
            #endif
        #endif // GPIO_PIN_LED_RED
        #if defined(TARGET_RX)
            #ifdef GPIO_PIN_LED_GREEN
                digitalWrite(GPIO_PIN_LED_GREEN, HIGH ^ GPIO_LED_GREEN_INVERTED);
            #endif

            #ifdef GPIO_PIN_LED_RED
                digitalWrite(GPIO_PIN_LED_RED, HIGH ^ GPIO_LED_RED_INVERTED);
            #endif

            #ifdef GPIO_PIN_LED
                if (connectionHasModelMatch)
                {
                    digitalWrite(GPIO_PIN_LED, HIGH ^ GPIO_LED_RED_INVERTED); // turn on led
                }
                else
                {
                    return flashLED(GPIO_PIN_LED, GPIO_LED_RED_INVERTED, LEDSEQ_MODEL_MISMATCH, sizeof(LEDSEQ_MODEL_MISMATCH));
                }
            #endif
        #endif
        return DURATION_NEVER;
    case disconnected:
        #if defined(TARGET_TX)
            #if defined(TARGET_TX_IFLIGHT)
                digitalWrite(GPIO_PIN_LED_GREEN, LOW);
                digitalWrite(GPIO_PIN_LED_BLUE, LOW);
                return flashLED(GPIO_PIN_LED_RED, GPIO_LED_RED_INVERTED, LEDSEQ_DISCONNECTED, sizeof(LEDSEQ_DISCONNECTED));
            #elif defined(GPIO_PIN_LED_RED) && (GPIO_PIN_LED_RED != UNDEF_PIN)
                digitalWrite(GPIO_PIN_LED_RED, LOW ^ GPIO_LED_RED_INVERTED);
            #endif
        #endif
        #if defined(TARGET_RX)
            #ifdef GPIO_PIN_LED_GREEN
                digitalWrite(GPIO_PIN_LED_GREEN, LOW ^ GPIO_LED_GREEN_INVERTED);
            #endif
            #ifdef GPIO_PIN_LED_RED
                digitalWrite(GPIO_PIN_LED_RED, LOW ^ GPIO_LED_RED_INVERTED);
            #endif
            #ifdef GPIO_PIN_LED
                return flashLED(GPIO_PIN_LED, GPIO_LED_RED_INVERTED, LEDSEQ_DISCONNECTED, sizeof(LEDSEQ_DISCONNECTED));
            #elif GPIO_PIN_LED_GREEN
                return flashLED(GPIO_PIN_LED_GREEN, GPIO_LED_GREEN_INVERTED, LEDSEQ_DISCONNECTED, sizeof(LEDSEQ_DISCONNECTED));
            #endif
        #endif
        return DURATION_NEVER;
    case wifiUpdate:
        #if defined(TARGET_TX_IFLIGHT)
            digitalWrite(GPIO_PIN_LED_GREEN, LOW);
            digitalWrite(GPIO_PIN_LED_RED, LOW);
            return flashLED(GPIO_PIN_LED_BLUE, GPIO_LED_BLUE_INVERTED, LEDSEQ_WIFI_UPDATE, sizeof(LEDSEQ_WIFI_UPDATE));
        #elif defined(TARGET_RX) && defined(GPIO_PIN_LED)
            return flashLED(GPIO_PIN_LED, GPIO_LED_RED_INVERTED, LEDSEQ_WIFI_UPDATE, sizeof(LEDSEQ_WIFI_UPDATE));
        #elif defined(GPIO_PIN_LED_RED) && (GPIO_PIN_LED_RED != UNDEF_PIN)
            return flashLED(GPIO_PIN_LED_RED, GPIO_LED_RED_INVERTED, LEDSEQ_WIFI_UPDATE, sizeof(LEDSEQ_WIFI_UPDATE));
        #else
            return DURATION_NEVER;
        #endif
    case noCrossfire: // technically nocrossfire is {10,100} but {20,100} is close enough
    case radioFailed:
        #if defined(TARGET_TX_IFLIGHT)
            digitalWrite(GPIO_PIN_LED_GREEN, LOW);
            digitalWrite(GPIO_PIN_LED_BLUE, LOW);
            return flashLED(GPIO_PIN_LED_RED, GPIO_LED_RED_INVERTED, LEDSEQ_RADIO_FAILED, sizeof(LEDSEQ_RADIO_FAILED));
        #elif defined(GPIO_PIN_LED_GREEN) && (GPIO_PIN_LED_GREEN != UNDEF_PIN)
            digitalWrite(GPIO_PIN_LED_GREEN, LOW ^ GPIO_LED_GREEN_INVERTED);
        #endif // GPIO_PIN_LED_GREEN
        #if defined(GPIO_PIN_LED_RED) && (GPIO_PIN_LED_RED != UNDEF_PIN)
            return flashLED(GPIO_PIN_LED_RED, GPIO_LED_RED_INVERTED, LEDSEQ_RADIO_FAILED, sizeof(LEDSEQ_RADIO_FAILED));
        #elif defined(GPIO_PIN_LED) && (GPIO_PIN_LED != UNDEF_PIN)
            return flashLED(GPIO_PIN_LED, GPIO_LED_RED_INVERTED, LEDSEQ_RADIO_FAILED, sizeof(LEDSEQ_RADIO_FAILED));
        #else
            return DURATION_NEVER;
        #endif
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

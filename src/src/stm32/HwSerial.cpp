#include "HwSerial.h"
#include "targets.h"

#if defined(GPIO_PIN_RCSIGNAL_RX) && defined(GPIO_PIN_RCSIGNAL_TX)
#define SPORT_RX_TX GPIO_PIN_RCSIGNAL_RX, GPIO_PIN_RCSIGNAL_TX
#else
#if (SPORT == PB10)
// USART3
#define SPORT_RX_TX PB11, SPORT
#elif (SPORT == PA2)
// USART2
#define SPORT_RX_TX PA3, SPORT
#elif (SPORT == PA9)
// USART1
#define SPORT_RX_TX PA10, SPORT
#else
#error "No valid S.Port UART TX pin defined!"
#endif
#endif

HwSerial CrsfSerial(SPORT_RX_TX, BUFFER_OE);

HwSerial::HwSerial(uint32_t _rx, uint32_t _tx, int32_t pin)
    : HardwareSerial(_rx, _tx), duplex_pin(pin)
{
}

HwSerial::HwSerial(void *peripheral, int32_t pin)
    : HardwareSerial(peripheral), duplex_pin(pin)
{
}

void HwSerial::Begin(uint32_t baud, uint32_t config)
{
    HardwareSerial::begin((unsigned long)baud, (uint8_t)config);
    if (duplex_pin > -1)
        pinMode(duplex_pin, OUTPUT);
}

void HwSerial::enable_receiver(void)
{
    if (duplex_pin > -1)
    {
        digitalWrite(duplex_pin, LOW);
        HAL_HalfDuplex_EnableReceiver(&_serial.handle);
    }
}

void HwSerial::enable_transmitter(void)
{
    delayMicroseconds(20);
    if (duplex_pin > -1)
    {
        HAL_HalfDuplex_EnableTransmitter(&_serial.handle);
        digitalWrite(duplex_pin, HIGH);
    }
}

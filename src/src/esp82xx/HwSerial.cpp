#include "HwSerial.h"
#include "targets.h"

#ifndef CRSF_SERIAL_NBR
#define CRSF_SERIAL_NBR 0
#elif (CRSF_SERIAL_NBR > 2)
#error "Not supported serial!"
#endif

HwSerial CrsfSerial(CRSF_SERIAL_NBR, -1);

HwSerial::HwSerial(int uart_nr, int32_t pin) : HardwareSerial(uart_nr)
{
    duplex_pin = pin;
}

void HwSerial::Begin(uint32_t baud, uint32_t config)
{
    HardwareSerial::begin(baud, (enum SerialConfig)config);
    enable_receiver();
    HardwareSerial::flush();
}

void HwSerial::enable_receiver(void)
{
}

void HwSerial::enable_transmitter(void)
{
}

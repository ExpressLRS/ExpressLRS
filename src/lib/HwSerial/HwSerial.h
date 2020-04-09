#ifndef SERIAL_H_
#define SERIAL_H_

#include <HardwareSerial.h>

class HwSerial : public HardwareSerial
{
public:
#if defined(PLATFORM_ESP32) || defined(PLATFORM_ESP8266)
    HwSerial(int uart_nr) : HardwareSerial(uart_nr){};
#else
    HwSerial(uint32_t _rx, uint32_t _tx) : HardwareSerial(_rx, _tx){};
    HwSerial(PinName _rx, PinName _tx) : HardwareSerial(_rx, _tx){};
    HwSerial(void *peripheral) : HardwareSerial(peripheral){};
#endif

#ifdef __STM32F1xx_HAL_UART_H
    void begin(unsigned long baud)
    {
        HwSerial::begin(baud, SERIAL_8N1);
    }
    void begin(unsigned long baud, uint8_t mode)
    {
        HardwareSerial::begin(baud, mode);
        enable_receiver();
    }
#endif

    void flush_read(void)
    {
        while (available())
            read();
    }

    void enable_receiver(void)
    {
#ifdef __STM32F1xx_HAL_UART_H
        HAL_HalfDuplex_EnableReceiver(&_serial.handle);
#endif
    }
    void enable_transmitter(void)
    {
#ifdef __STM32F1xx_HAL_UART_H
        HAL_HalfDuplex_EnableTransmitter(&_serial.handle);
#endif
    }
};

#endif /* SERIAL_H_ */

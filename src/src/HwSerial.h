#ifndef SERIAL_H_
#define SERIAL_H_

#include <HardwareSerial.h>
#include "platform.h"

class HwSerial : public HardwareSerial
{
public:
    HwSerial(int uart_nr, int32_t duplex_pin = -1);
    HwSerial(uint32_t _rx, uint32_t _tx, int32_t duplex_pin = -1);
    HwSerial(void *peripheral, int32_t duplex_pin = -1);

    void Begin(uint32_t baud, uint32_t config = SERIAL_8N1);

    void ICACHE_RAM_ATTR enable_receiver(void);
    void ICACHE_RAM_ATTR enable_transmitter(void);

    void ICACHE_RAM_ATTR flush_read()
    {
        while (available())
            (void)read();
    }

    size_t write(const uint8_t *buff, size_t len)
    {
        size_t ret;
        enable_transmitter();
        ret = HardwareSerial::write(buff, len);
        enable_receiver();
        return ret;
    }

    size_t write_buffer(const uint8_t * buff, size_t len)
    {
        return HardwareSerial::write(buff, len);
    }

private:
};

extern HwSerial CrsfSerial;

#endif /* SERIAL_H_ */

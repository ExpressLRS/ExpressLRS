#pragma once

//#include "internal.h"
#include <stdint.h>
#include <stddef.h>

#define SERIAL_8N1 0x06

typedef struct
{
    void *handle;
} Serial_t;

/* Interface kept arduino compatible */
class HardwareSerial
{
public:
    HardwareSerial(uint32_t rx, uint32_t tx);
    HardwareSerial(void *peripheral);
    void begin(unsigned long baud, uint8_t mode = SERIAL_8N1);
    void end(void);
    virtual int available(void);
    virtual int peek(void);
    virtual int read(void);
    virtual void flush(void);
    virtual uint32_t write(uint8_t);
    virtual uint32_t write(const uint8_t *buff, uint32_t len);

    void HAL_HalfDuplex_EnableReceiver(void *);
    void HAL_HalfDuplex_EnableTransmitter(void *);

protected:
    Serial_t _serial;
    uint32_t rx_pin;
    uint32_t tx_pin;
    int32_t irq;
};

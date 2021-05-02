#pragma once

#include "FIFO.h"

class TransportLayer : public Stream
{
    HardwareSerial* _dev = nullptr;

    FIFO outFIFO;

#if defined(PLATFORM_ESP32) && defined(CRSF_TX_MODULE)
    portMUX_TYPE FIFOmux = portMUX_INITIALIZER_UNLOCKED;
#endif

    bool halfDuplex = false;

    void disableTX();
    void enableTX();
    
public:
    TransportLayer();

    void begin(HardwareSerial* dev, bool halfDuplex=false);
    void end();

    void setHalfDuplex() { halfDuplex = true; }
    void setFullDuplex() { halfDuplex = false; }

    void updateBaudRate(unsigned rate);
    
    void flushInput();
    void flushOutput();

    void flushFIFO() { outFIFO.flush(); }

    void sendAsync(uint8_t* pkt, uint8_t pkt_len);
    
    // Stream methods
    int available() override { return _dev ? _dev->available() : 0; }
    int read() { return _dev ? _dev->read() : 0; }
    int peek() { return _dev ? _dev->peek() : 0; }
    void flush() { flushOutput(); }

    // Print methods
    size_t write(uint8_t c) override
    { 
        return _dev ? _dev->write(c) : 0;
    }

    size_t write(uint8_t *c, int l)
    {
        return _dev ? _dev->write(c,l) : 0;
    }
};

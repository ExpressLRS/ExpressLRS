#pragma once

#include "targets.h"
#include "FIFO.h"

class SerialIO {
public:

    SerialIO(Stream *output, Stream *input) : _outputPort(output), _inputPort(input) {}
    virtual ~SerialIO() {}

    void setFailsafe(bool failsafe);
    virtual void sendLinkStatisticsToFC() = 0;
    virtual void sendMSPFrameToFC(uint8_t* data) = 0;

    virtual uint32_t sendRCFrameToFC(bool frameAvailable, uint32_t *channelData) = 0;
    virtual void handleUARTout() {};
    virtual void handleUARTin();

protected:
    const int defaultMaxSerialReadSize = 64;

    Stream *_outputPort;
    Stream *_inputPort;
    bool failsafe = false;

    virtual int getMaxSerialReadSize() { return defaultMaxSerialReadSize; }
    virtual void processBytes(uint8_t *bytes, uint16_t size) = 0;
};

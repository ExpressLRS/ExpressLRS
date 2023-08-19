#pragma once

#include "targets.h"
#include "FIFO.h"

class SerialIO {
public:
    const int defaultMaxSerialReadSize = 64;

    SerialIO(Stream *output, Stream *input) : _outputPort(output), _inputPort(input) {}
    virtual ~SerialIO() {}

    virtual void setLinkQualityStats(uint16_t lq, uint16_t rssi) = 0;
    virtual void sendLinkStatisticsToFC() = 0;
    virtual void sendMSPFrameToFC(uint8_t* data) = 0;
    virtual uint32_t sendRCFrameToFC(bool frameAvailable, uint32_t *channelData) = 0;

    virtual int getMaxSerialReadSize() { return defaultMaxSerialReadSize; }

    virtual void handleUARTout() {};
    virtual void handleUARTin();

    void setFailsafe(bool failsafe);

protected:
    Stream *_outputPort;
    Stream *_inputPort;
    bool failsafe = false;

    virtual void processBytes(uint8_t *bytes, uint16_t size);
    virtual void processByte(uint8_t byte) = 0;
};

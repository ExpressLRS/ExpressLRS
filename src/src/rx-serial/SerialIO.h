#pragma once

#include "targets.h"
#include "FIFO.h"

class SerialIO {
public:
    SerialIO(Stream *output, Stream *input) : _outputPort(output), _inputPort(input) {}
    virtual ~SerialIO() {}

    virtual void setLinkQualityStats(uint16_t lq, uint16_t rssi) = 0;
    virtual void sendLinkStatisticsToFC() = 0;
    virtual void sendMSPFrameToFC(uint8_t* data) = 0;

    virtual uint32_t sendRCFrameToFC(bool frameAvailable, uint32_t *channelData) = 0;

    virtual void handleUARTout();
    virtual void handleUARTin();

protected:
    Stream *_outputPort;
    Stream *_inputPort;
    FIFO _fifo;

    virtual void processByte(uint8_t byte) = 0;
};

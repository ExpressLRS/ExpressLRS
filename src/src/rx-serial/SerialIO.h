#pragma once

#include "targets.h"
#include "FIFO.h"

class SerialIO {
public:

    SerialIO(Stream *output, Stream *input) : _outputPort(output), _inputPort(input) {}
    virtual ~SerialIO() {}

    void setFailsafe(bool failsafe);

    virtual void queueLinkStatisticsPacket() = 0;
    virtual void queueMSPFrameTransmission(uint8_t* data) = 0;

    virtual uint32_t sendRCFrame(bool frameAvailable, uint32_t *channelData) = 0;

    virtual void sendQueuedData(uint32_t maxBytesToSend);
    virtual void processSerialInput();
    virtual int getMaxSerialWriteSize() { return defaultMaxSerialWriteSize; }

protected:
    const int defaultMaxSerialReadSize = 64;
    const int defaultMaxSerialWriteSize = 128;

    Stream *_outputPort;
    Stream *_inputPort;
    bool failsafe = false;

    static const uint32_t SERIAL_OUTPUT_FIFO_SIZE = 256U;
    FIFO<SERIAL_OUTPUT_FIFO_SIZE> _fifo;

    virtual int getMaxSerialReadSize() { return defaultMaxSerialReadSize; }
    virtual void processBytes(uint8_t *bytes, uint16_t size) = 0;
};

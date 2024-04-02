#include "SerialIO.h"

void SerialIO::setFailsafe(bool failsafe)
{
    this->failsafe = failsafe;
}

void SerialIO::processSerialInput()
{
    auto maxBytes = getMaxSerialReadSize();
    uint8_t buffer[maxBytes];
    auto size = min(_inputPort->available(), maxBytes);
    _inputPort->readBytes(buffer, size);
    processBytes(buffer, size);
}

void SerialIO::sendQueuedData(uint32_t maxBytesToSend)
{
    uint32_t bytesWritten = 0;

    while (_fifo.size() > _fifo.peek() && (bytesWritten + _fifo.peek()) < maxBytesToSend)
    {
        _fifo.lock();
        uint8_t OutPktLen = _fifo.pop();
        uint8_t OutData[OutPktLen];
        _fifo.popBytes(OutData, OutPktLen);
        _fifo.unlock();
        this->_outputPort->write(OutData, OutPktLen); // write the packet out
        bytesWritten += OutPktLen;
    }
}

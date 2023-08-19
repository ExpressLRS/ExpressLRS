#include "SerialIO.h"

void SerialIO::handleUARTin()
{
    auto maxBytes = getMaxSerialReadSize();
    uint8_t buffer[maxBytes];
    auto size = min(_inputPort->available(), maxBytes);
    _inputPort->readBytes(buffer, size);
    processBytes(buffer, size);
}

void SerialIO::setFailsafe(bool failsafe)
{
    this->failsafe = failsafe;
}

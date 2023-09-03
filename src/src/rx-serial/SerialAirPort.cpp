#if defined(TARGET_RX)

#include "SerialAirPort.h"
#include "device.h"
#include "common.h"

// Variables / constants for Airport //
FIFO<AP_MAX_BUF_LEN> apInputBuffer;
FIFO<AP_MAX_BUF_LEN> apOutputBuffer;


void SerialAirPort::setLinkQualityStats(uint16_t lq, uint16_t rssi)
{
    // unsupported
}

void SerialAirPort::sendLinkStatisticsToFC()
{
    // unsupported
}

uint32_t SerialAirPort::sendRCFrameToFC(bool frameAvailable, uint32_t *channelData)
{
    return DURATION_IMMEDIATELY;
}

void SerialAirPort::sendMSPFrameToFC(uint8_t* data)
{
    // unsupported
}

int SerialAirPort::getMaxSerialReadSize()
{
    return AP_MAX_BUF_LEN - apInputBuffer.size();
}

void SerialAirPort::processBytes(uint8_t *bytes, u_int16_t size)
{
    if (connectionState == connected)
    {
        apInputBuffer.atomicPushBytes(bytes, size);
    }
}

void SerialAirPort::handleUARTout()
{
    apOutputBuffer.lock();
    auto size = apOutputBuffer.size();
    uint8_t buf[size];
    apOutputBuffer.popBytes(buf, size);
    apOutputBuffer.unlock();
    _outputPort->write(buf, size);
}
#endif

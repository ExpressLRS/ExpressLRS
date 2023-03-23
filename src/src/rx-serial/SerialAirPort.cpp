#if defined(TARGET_RX)

#include "SerialAirPort.h"
#include "device.h"
#include "common.h"

// Variables / constants for Airport //
FIFO_GENERIC<AP_MAX_BUF_LEN> apInputBuffer;
FIFO_GENERIC<AP_MAX_BUF_LEN> apOutputBuffer;


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

void SerialAirPort::processByte(uint8_t byte)
{
    if (apInputBuffer.size() < AP_MAX_BUF_LEN && connectionState == connected)
    {
        apInputBuffer.push(byte);
    }
}

void SerialAirPort::handleUARTout()
{
    while (apOutputBuffer.size())
    {
        _outputPort->write(apOutputBuffer.pop());
    }
}
#endif

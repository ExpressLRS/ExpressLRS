#include "SerialIO.h"

#if defined(USE_MSP_WIFI)
#include "crsf2msp.h"
#include "msp2crsf.h"

extern CROSSFIRE2MSP crsf2msp;
extern MSP2CROSSFIRE msp2crsf;
#endif

void SerialIO::handleUARTout()
{
    // don't write more than 128 bytes at a time to avoid RX buffer overflow
    const int maxBytesPerCall = 128;
    uint32_t bytesWritten = 0;
    #if defined(USE_MSP_WIFI)
        while (msp2crsf.FIFOout.size() > msp2crsf.FIFOout.peek() && (bytesWritten + msp2crsf.FIFOout.peek()) < maxBytesPerCall)
        {
            uint8_t OutPktLen = msp2crsf.FIFOout.pop();

            uint8_t OutData[OutPktLen];
            msp2crsf.FIFOout.popBytes(OutData, OutPktLen);
            this->_outputPort->write(OutData, OutPktLen); // write the packet out
            bytesWritten += OutPktLen;
        }
    #endif

    while (_fifo.size() > _fifo.peek() && (bytesWritten + _fifo.peek()) < maxBytesPerCall)
    {
        noInterrupts();
        uint8_t OutPktLen = _fifo.pop();
        uint8_t OutData[OutPktLen];
        _fifo.popBytes(OutData, OutPktLen);
        interrupts();
        this->_outputPort->write(OutData, OutPktLen); // write the packet out
        bytesWritten += OutPktLen;
    }
}


void SerialIO::handleUARTin()
{
    while (_inputPort->available())
    {
        uint8_t byte = _inputPort->read();
        processByte(byte);
    }
}

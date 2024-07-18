#include "SerialTramp.h"
#include "msptypes.h"
#include "freqTable.h"
#include <hal/uart_ll.h>

void SerialTramp::setTXMode()
{
#if defined(PLATFORM_ESP32)
    pinMode(halfDuplexPin, OUTPUT);                                 // set half duplex GPIO to OUTPUT
    digitalWrite(halfDuplexPin, HIGH);                              // set half duplex GPIO to high level
    pinMatrixOutAttach(halfDuplexPin, UTXDoutIdx, false, false);    // attach GPIO as output of UART TX
#endif
}

void SerialTramp::setRXMode()
{
#if defined(PLATFORM_ESP32)
    pinMode(halfDuplexPin, INPUT_PULLUP);                           // set half duplex GPIO to INPUT
    pinMatrixInAttach(halfDuplexPin, URXDinIdx, false);             // attach half duplex GPIO as input to UART RX
#endif
}

// Calculate tramp protocol checksum of provided buffer
uint8_t checksum(uint8_t *buf)
{
    uint8_t cksum = 0;

    for (int i = 1 ; i < TRAMP_FRAME_SIZE - 2; i++) {
        cksum += buf[i];
    }

    return cksum;
}

void SerialTramp::sendQueuedData(uint32_t maxBytesToSend)
{
    uint32_t bytesWritten = 0;
    static unsigned long lastSendTime = 0; // OVTX only changes protocols on startup every 500ms; if we send our 3 packets in different 500ms windows, we have a better chance of success
    while (_fifo.size() > 0 && bytesWritten < maxBytesToSend && millis() - lastSendTime > 200){
        _fifo.lock();
        uint8_t frameSize = _fifo.pop() - 1;
        uint8_t frame[frameSize];
        _fifo.popBytes(frame, frameSize);
        _fifo.unlock();
        setTXMode();
        _outputPort->write(frame, frameSize);
        bytesWritten += frameSize;
        lastSendTime = millis();
    }
    if (_fifo.size() == 0 && uart_ll_is_tx_idle(UART_LL_GET_HW(1))) {
        setRXMode();
    }
}

// Up to us how we want to define these; official Tramp VTXes are 600mW max but other non-IRC VTXes
// implementing Tramp might support more. This seems like a reasonable tradeoff.
// In Lua, we have 1-8, so we'll define those here and leave 0=0.
uint16_t powerLevelLUT[9] = { 0, 10, 25, 200, 400, 600, 1000, 1600, 3000 };

void SerialTramp::queueMSPFrameTransmission(uint8_t* data)
{
    // What we're handed here is MSP wrapped in CRSF, so our offsets are thrown off
    uint8_t innerLength = data[6];
    if (innerLength < 2) {
        return;
    }
    if (data[7] != MSP_SET_VTX_CONFIG) {
        return;
    }

    // We're gonna lie and queue the MSP frame by transforming it into a handful of Tramp frames
    uint16_t freq;
    if (data[8] > VTXCOMMON_MSP_BANDCHAN_CHKVAL) {
        // This is a frequency in MHz
        freq = data[8] + (data[9] << 8);
    }
    else
    {
        freq = getFreqByIdx(data[8]);
    }
    if (freq == 0) {
        return;
    }

    // We need to construct Tramp packets from the MSP_SET_VTX_CONFIG packet
    uint8_t tempFrame[TRAMP_FRAME_SIZE];
    uint8_t frameIndex;
    // First, construct an 'F' type frame to set frequency - Tramp always uses frequency, not band/channel
    memset(tempFrame, 0, TRAMP_FRAME_SIZE);
    frameIndex = 0;
    tempFrame[frameIndex++] = TRAMP_HEADER;
    tempFrame[frameIndex++] = 'F';
    tempFrame[frameIndex++] = freq & 0xFF;
    tempFrame[frameIndex++] = (freq >> 8) & 0xFF;
    tempFrame[14] = checksum(tempFrame);
    _fifo.lock();
    _fifo.push(TRAMP_FRAME_SIZE + 1);
    _fifo.pushBytes(tempFrame, TRAMP_FRAME_SIZE);
    _fifo.unlock();

    // If packet has more than 4 bytes it also contains power idx and pitmode.
    bool havePowerAndPitmode = innerLength >= 4;
    if (havePowerAndPitmode) 
    {
        uint8_t powerIndex = data[10];
        // Bounds check the LUT
        uint8_t maxPowerIndex = sizeof(powerLevelLUT) / sizeof(powerLevelLUT[0]) - 1;
        if (powerIndex > maxPowerIndex) {
            powerIndex = maxPowerIndex;
        }

        // Set power
        uint16_t power = powerLevelLUT[powerIndex]; // Convert powerIndex to mW
        // Construct a 'P' type frame to set power
        memset(tempFrame, 0, TRAMP_FRAME_SIZE);
        frameIndex = 0;
        tempFrame[frameIndex++] = TRAMP_HEADER;
        tempFrame[frameIndex++] = 'P';
        tempFrame[frameIndex++] = power & 0xFF;
        tempFrame[frameIndex++] = (power >> 8) & 0xFF;
        tempFrame[14] = checksum(tempFrame);
        _fifo.lock();
        _fifo.push(TRAMP_FRAME_SIZE + 1);
        _fifo.pushBytes(tempFrame, TRAMP_FRAME_SIZE);
        _fifo.unlock();

        // Set pitmode
        uint8_t pitmode = data[11];
        // Construct an 'I' type frame to set pitmode
        memset(tempFrame, 0, TRAMP_FRAME_SIZE);
        frameIndex = 0;
        tempFrame[frameIndex++] = TRAMP_HEADER;
        tempFrame[frameIndex++] = 'I';
        tempFrame[frameIndex++] = pitmode ? 0 : 1; // Tramp uses inverted logic for pitmode
        tempFrame[14] = checksum(tempFrame);
        _fifo.lock();
        _fifo.push(TRAMP_FRAME_SIZE + 1);
        _fifo.pushBytes(tempFrame, TRAMP_FRAME_SIZE);
        _fifo.unlock();
    }
}
#include "SerialTramp.h"
#include "msptypes.h"
#include "freqTable.h"

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
    while (_fifo.size() > 0 && bytesWritten < maxBytesToSend) {
        _fifo.lock();
        uint8_t frameSize = _fifo.pop();
        uint8_t frame[frameSize];
        _fifo.popBytes(frame, frameSize);
        _fifo.unlock();
        _outputPort->write(frame, frameSize);
        bytesWritten += frameSize;
    }
}

// Up to us how we want to define these; official Tramp VTXes are 600mW max but other non-IRC VTXes
// implementing Tramp might support more. This seems like a reasonable tradeoff.
uint16_t powerLevelLUT[6] = { 0, 25, 200, 400, 600, 1000 };

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
    freq = getFreqByIdx(data[8]);
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
#include "SerialRLLLC.h"

#include <crsf_protocol.h>

// Serial 9600 8N1 with frame rate 50Hz
// Rlaarlo light control(RLLLC) frame format (6 bytes):
// [0] = 0x0F       Header
// [1] = 0x00..0xFF LC channel 1 (ST)
// [2] = 0x00..0xFF LC channel 2 (TH)
// [3] = 0x00..0xFF LC channel 3 (LC)
// [4] = 0x00..0xFF LC channel 4 (Winch)
// [5] = checksum   (sum of bytes 0..4) & 0xFF

static constexpr uint8_t LC_FRAME_SIZE = 6;
static constexpr uint8_t LC_FRAME_HEADER = 0x0F;
static constexpr uint32_t LC_FRAME_INTERVAL_MS = 20; // 50Hz

uint8_t SerialRLLLC::mapCrsfToByte(uint32_t value)
{
    const uint32_t constrainedValue = constrain(value, CRSF_CHANNEL_VALUE_MIN, CRSF_CHANNEL_VALUE_MAX);
    return fmap(constrainedValue, CRSF_CHANNEL_VALUE_MIN, CRSF_CHANNEL_VALUE_MAX, 0, 0xFF);
}

uint32_t SerialRLLLC::sendRCFrame(bool frameAvailable, bool frameMissed, uint32_t *channelData)
{
    (void)frameAvailable;
    (void)frameMissed;

    if (_outputPort->availableForWrite() < LC_FRAME_SIZE) {
        return LC_FRAME_INTERVAL_MS;
    }

    uint8_t LCFrame[LC_FRAME_SIZE];
    LCFrame[0] = LC_FRAME_HEADER;               // Header
    LCFrame[1] = mapCrsfToByte(channelData[0]); // Map CRSF channel 1 to LC channel 1 (ST)
    LCFrame[2] = mapCrsfToByte(channelData[1]); // Map CRSF channel 2 to LC channel 2 (TH)
    LCFrame[3] = mapCrsfToByte(channelData[7]); // Map CRSF channel 8 to LC channel 3 (LC)
    LCFrame[4] = mapCrsfToByte(channelData[8]); // Map CRSF channel 9 to LC channel 4 (Winch)
    LCFrame[5] = (LCFrame[0] + LCFrame[1] + LCFrame[2] + LCFrame[3] + LCFrame[4]) & 0xFF; // Checksum

    _outputPort->write(LCFrame, LC_FRAME_SIZE);

    return LC_FRAME_INTERVAL_MS;
}

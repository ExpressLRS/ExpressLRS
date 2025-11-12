#pragma once

#include <functional>

#include "crc.h"
#include "crsfmsp_common.h"

/*  Takes a CRSF(MSP) frame and converts it to raw MSP frame
    adding the MSP header and checksum. Handles chunked MSP messages.
*/

class CROSSFIRE2MSP final
{
public:
    void parse(const uint8_t *data, const std::function<void(uint8_t *, uint32_t)> &processMSP); // accept crsf frame input
    const uint8_t *getFrame() const { return outBuffer; }
    uint32_t getFrameLen() const { return idx + 1; } // include the last byte (crc)
    void reset();

private:
    uint8_t outBuffer[MSP_FRAME_MAX_LEN] {};
    uint32_t pktLen = 0; // packet length of the incoming msp frame
    uint32_t idx = 0;    // number of bytes received in the current msp frame
    uint8_t seqNumberPrev = 0;
    bool frameComplete = false;
    uint8_t src = 0;                            // source of the msp frame (from CRSF ext header)
    uint8_t dest = 0;                           // destination of the msp frame (from CRSF ext header)
    MSPframeType_e MSPvers = MSP_FRAME_UNKNOWN; // need to store the MSP version since it can only be inferred from the first frame

    static bool isNewFrame(const uint8_t *data);
    static bool isError(const uint8_t *data);

    static uint8_t getSeqNumber(const uint8_t *data);
    static MSPframeType_e getVersion(const uint8_t *data);
    static uint8_t getHeaderDir(const uint8_t *data);
    static uint8_t getChecksum(const uint8_t *data, uint32_t packetLen, MSPframeType_e mspVersion);
    static uint32_t getFrameLen(const uint8_t *data, MSPframeType_e mspVersion);
};

#pragma once

#include <cstdint>
#include "FIFO_GENERIC.h"
#include "crsfmsp_common.h"
#include "crc.h"
#include "logging.h"

/*  Takes a CRSF(MSP) frame and converts it to raw MSP frame
    adding the MSP header and checksum. Handles chunked MSP messages.
*/

class CROSSFIRE2MSP
{
private:
    uint8_t outBuffer[MSP_FRAME_MAX_LEN];
    uint32_t pktLen; // packet length of the incomming msp frame
    uint32_t idx;    // number of bytes received in the current msp frame
    uint8_t seqNumberPrev;
    bool frameComplete;
    uint8_t src;            // source of the msp frame (from CRSF ext header)
    uint8_t dest;           // destination of the msp frame (from CRSF ext header)
    MSPframeType_e MSPvers; // need to store the MSP version since it can only be inferred from the first frame

    bool isNewFrame(const uint8_t *data);
    bool isError(const uint8_t *data);

    uint8_t getSeqNumber(const uint8_t *data);
    MSPframeType_e getVersion(const uint8_t *data);
    uint8_t getHeaderDir(const uint8_t *data);
    uint8_t getChecksum(const uint8_t *data, const uint32_t packetLen, MSPframeType_e mspVersion);
    uint32_t getFrameLen(const uint8_t *data, MSPframeType_e mspVersion);

public:
    CROSSFIRE2MSP();
    FIFO_GENERIC<MSP_FRAME_MAX_LEN> FIFOout;
    void parse(const uint8_t *data); // accept crsf frame input
    bool isFrameReady();
    const uint8_t *getFrame();
    uint32_t getFrameLen();
    void reset();
    uint8_t getSrc();
    uint8_t getDest();
};

extern CROSSFIRE2MSP crsf2msp;

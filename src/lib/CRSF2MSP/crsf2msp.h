#pragma once

#include <cstdint>
#include <cstring>
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
    uint32_t pktLen;    // packet length of the incomming msp frame
    uint32_t idx; // number of bytes received in the current msp frame
    bool frameComplete;
    uint8_t src;  // source of the msp frame (from CRSF ext header)
    uint8_t dest; // destination of the msp frame (from CRSF ext header)

    bool isNewFrame(uint8_t data);
    bool isError(uint8_t data);

    uint8_t getSeqNumber(uint8_t data);
    uint8_t getVersion(uint8_t data);
    uint8_t getHeaderDir(uint8_t data);
    uint8_t getChecksum(const uint8_t *data, uint8_t mspVersion);
    uint32_t getFrameLen(const uint8_t *data, uint8_t mspVersion);

public:
    CROSSFIRE2MSP();
    void parse(const uint8_t *data, uint8_t len); // accept crsf frame input
    bool isFrameReady();
    const uint8_t *getFrame();
    uint32_t getFrameLen();
    uint8_t getSrc();
    uint8_t getDest();
};

#pragma once

#include <cstdint>
#include "FIFO_GENERIC.h"
#include "crsfmsp_common.h"
#include "crsf_protocol.h"
#include "crc.h"
#include "logging.h"

/* Takes a MSP frame and converts it to raw CRSF frame
   adding the CRSF header and checksum. Handles chunking of messages
*/

class MSP2CROSSFIRE
{
private:
    // bool isBusy;
    void setNewFrame(uint8_t &data, bool isNewFrame);
    void setSeqNumber(uint8_t &data, uint8_t seqNumber);
    void setVersion(uint8_t &data, MSPframeType_e version);
    uint8_t getHeaderDir(uint8_t headerDir);
    void setError(uint8_t &data, bool isError);
    uint8_t seqNum;

    uint32_t getFrameLen(uint32_t payloadLen, uint8_t mspVersion);
    MSPframeType_e getVersion(const uint8_t *data);
    uint32_t getPayloadLen(const uint8_t *data, MSPframeType_e mspVersion);

public:
    MSP2CROSSFIRE();
    FIFO_GENERIC<MSP_FRAME_MAX_LEN> FIFOout;
    void parse(const uint8_t *data, uint32_t frameLen, uint8_t src = CRSF_ADDRESS_CRSF_RECEIVER, uint8_t dest = CRSF_ADDRESS_FLIGHT_CONTROLLER);
    bool validate(const uint8_t *data, uint32_t expectLen);
};

extern MSP2CROSSFIRE msp2crsf;

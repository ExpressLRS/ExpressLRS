#pragma once

#include <cstdint>
#include <cstring>
#include "crsfmsp_common.h"
#include "crsf_protocol.h"
#include "crc.h"
#include "FIFO_GENERIC.h"
//#include "logging.h"

/* Takes a MSP frame and converts it to raw CRSF frame
   adding the CRSF header and checksum. Handles chunking of messages
*/

class MSP2CROSSFIRE
{
private:
    // bool isBusy;
    void setNewFrame(uint8_t &data, bool isNewFrame);
    void setSeqNumber(uint8_t &data, uint8_t seqNumber);
    void setVersion(uint8_t &data, uint8_t version);
    uint8_t getHeaderDir(uint8_t headerDir);
    void setError(uint8_t &data, bool isError);

    uint8_t getV1payloadLen(const uint8_t *data);
    uint16_t getV2payloadLen(const uint8_t *data);

public:
    MSP2CROSSFIRE();
    FIFO_GENERIC<MSP_FRAME_MAX_LEN> FIFOout;
    void parse(const uint8_t *data, uint32_t frameLen, uint8_t src = CRSF_ADDRESS_CRSF_RECEIVER, uint8_t dest = CRSF_ADDRESS_FLIGHT_CONTROLLER);
};

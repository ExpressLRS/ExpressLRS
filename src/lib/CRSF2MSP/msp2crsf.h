#pragma once

#include "CRSFConnector.h"
#include "FIFO.h"
#include "crsfmsp_common.h"

/* Takes an MSP frame and converts it to a raw CRSF frame
   adding the CRSF header and checksum. Handles chunking of messages
*/

class MSP2CROSSFIRE final
{
public:
    MSP2CROSSFIRE() = default;
    void parse(CRSFConnector *connector, const uint8_t *data, uint32_t frameLen, crsf_addr_e src = CRSF_ADDRESS_CRSF_RECEIVER, crsf_addr_e dest = CRSF_ADDRESS_FLIGHT_CONTROLLER);
    static bool validate(const uint8_t *data, uint32_t expectLen);

private:
    uint8_t seqNum = 0;

    static void setNewFrame(uint8_t &data, bool isNewFrame);
    static void setSeqNumber(uint8_t &data, uint8_t seqNumber);
    static void setVersion(uint8_t &data, MSPframeType_e version);
    static crsf_frame_type_e getHeaderDir(uint8_t headerDir);
    static void setError(uint8_t &data, bool isError);

    static uint32_t getFrameLen(uint32_t payloadLen, uint8_t mspVersion);
    static MSPframeType_e getVersion(const uint8_t *data);
    static uint32_t getPayloadLen(const uint8_t *data, MSPframeType_e mspVersion);
};

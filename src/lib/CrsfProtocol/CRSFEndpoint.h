#ifndef CRSF_ENDPOINT_H
#define CRSF_ENDPOINT_H

#include "CRSFConnector.h"
#include "crsf_protocol.h"
#include "msp.h"

#include <vector>

class CRSFEndpoint {
public:
    explicit CRSFEndpoint(const uint8_t device_id)
        : device_id(device_id) {}

    virtual ~CRSFEndpoint() = default;

    virtual bool handleMessage(crsf_header_t * message) = 0;

    /* Process the message if it's for our device_id or a broadcast.
     * If the message is not for us, or it's a broadcast message, then forward it to all 'other' connectors.
     * The message will be delivered to the connector that 'hosts' the destination or all other connectors if this is a broadcast message.
     * As messages are added, a routing table is created with which device_ids are available via each connector.
     */
    void processMessage(CRSFConnector *connector, crsf_header_t *message);

    void addConnector(CRSFConnector *connector);

    void SetHeaderAndCrc(uint8_t *frame, crsf_frame_type_e frameType, uint8_t frameSize, crsf_addr_e destAddr);
    void SetExtendedHeaderAndCrc(uint8_t *frame, crsf_frame_type_e frameType, uint8_t frameSize, crsf_addr_e senderAddr, crsf_addr_e destAddr);
    void makeLinkStatisticsPacket(uint8_t *buffer);
    void AddMspMessage(const mspPacket_t *packet, uint8_t destination, uint8_t origin);

    GENERIC_CRC8 crsf_crc = GENERIC_CRC8(CRSF_CRC_POLY);

    elrsLinkStatistics_t linkStats = {};

private:
    uint8_t device_id;
    std::vector<CRSFConnector *> connectors;
};

// The global instance of the endpoint
extern CRSFEndpoint *crsfEndpoint;

#endif //CRSF_ENDPOINT_H

#ifndef CRSF_ENDPOINT_H
#define CRSF_ENDPOINT_H

#include "CRSFConnector.h"
#include "crc.h"
#include "crsf_protocol.h"
#include "msp.h"

#include <vector>

class CRSFEndpoint {
public:
    explicit CRSFEndpoint(const crsf_addr_e device_id)
        : device_id(device_id) {}

    virtual ~CRSFEndpoint() = default;

    virtual bool handleRaw(uint8_t *message) { return false; }
    virtual bool handleMessage(const crsf_header_t * message) = 0;

    /* Process the message if it's for our device_id or a broadcast.
     * If the message is not for us, or it's a broadcast message, then forward it to all 'other' connectors.
     * The message will be delivered to the connector that 'hosts' the destination or all other connectors if this is a broadcast message.
     * As messages are added, a routing table is created with which device_ids are available via each connector.
     */
    void processMessage(CRSFConnector *connector, const crsf_header_t *message);

    void addConnector(CRSFConnector *connector);

    void SetMspV2Request(uint8_t *frame, uint16_t function, uint8_t *payload, uint8_t payloadLength);
    void SetHeaderAndCrc(crsf_header_t *frame, crsf_frame_type_e frameType, uint8_t frameSize, crsf_addr_e destAddr);
    void SetExtendedHeaderAndCrc(crsf_ext_header_t *frame, crsf_frame_type_e frameType, uint8_t frameSize, crsf_addr_e destAddr);
    void makeLinkStatisticsPacket(uint8_t *buffer);
    void AddMspMessage(const mspPacket_t *packet, uint8_t destination, uint8_t origin);

    GENERIC_CRC8 crsf_crc = GENERIC_CRC8(CRSF_CRC_POLY);

    elrsLinkStatistics_t linkStats = {};

private:
    crsf_addr_e device_id;
    std::vector<CRSFConnector *> connectors;
};

// The global instance of the endpoint
extern CRSFEndpoint *crsfEndpoint;

#endif //CRSF_ENDPOINT_H

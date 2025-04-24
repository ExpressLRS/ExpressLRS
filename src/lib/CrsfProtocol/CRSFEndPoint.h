#ifndef CRSF_ENDPOINT_H
#define CRSF_ENDPOINT_H

#include "CRSFConnector.h"
#include "crsf_protocol.h"
#include "msp.h"

#include <vector>

class CRSFEndPoint {
public:
    explicit CRSFEndPoint(const uint8_t device_id)
        : device_id(device_id) {}

    virtual ~CRSFEndPoint() = default;

    /* Process the message if it's for our device_id or a broadcast.
     * If the message is not for us, or it's a broadcast message, then forward it to all 'other' connectors.
     * The message will be delivered to the connector that 'hosts' the destination or all other connectors if this is a broadcast message.
     * As messages are added, a routing table is created with which device_ids are available via each connector.
     */
    void processMessage(CRSFConnector *connector, crsf_ext_header_t *message);

    void addConnector(CRSFConnector *connector);

    virtual bool handleMessage(crsf_ext_header_t * message) = 0;

    void AddMspMessage(mspPacket_t *packet, uint8_t destination);

    elrsLinkStatistics_t LinkStats = {};

private:
    uint8_t device_id;
    std::vector<CRSFConnector *> connectors;
};

#endif //CRSF_ENDPOINT_H

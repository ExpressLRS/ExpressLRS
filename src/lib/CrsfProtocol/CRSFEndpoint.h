#ifndef CRSF_ENDPOINT_H
#define CRSF_ENDPOINT_H

#include "CRSFConnector.h"
#include "crc.h"
#include "msp.h"

#include <vector>

class CRSFEndpoint {
public:
    explicit CRSFEndpoint(const crsf_addr_e device_id)
        : device_id(device_id) {}

    virtual ~CRSFEndpoint() = default;

    /**
     * Adds a CRSFConnector instance to the list of connectors managed by this CRSFEndpoint.
     *
     * This function allows the CRSFEndpoint to interface with and manage multiple CRSFConnector
     * objects, enabling message communication and routing.
     *
     * @param connector Pointer to the CRSFConnector that will be added to the endpoint's managed connectors.
     */
    void addConnector(CRSFConnector *connector);

    /**
     * Handles a raw message and determines if the message should be further processed or ignored.
     *
     * This function exists for messages which don't necessarily conform to the CRSF specification.
     *
     * All messages, regardless of routing information, are passed to this function.
     *
     * @param message Pointer to the CRSF message header structure containing the message data.
     * @return A boolean value indicating whether the message should be ignored and not processed further.
     */
    virtual bool handleRaw(const crsf_header_t * message) { return false; }

    /**
     * Handles a CRSF message and determines if it should be processed further or ignored.
     *
     * This function is responsible for processing messages with the CRSF header
     * and implementing specific logic for derived classes.
     *
     * Only extended messages that are destined for this endpoint, either matching the device_id
     * or by matching the broadcast device_id, will be passed to this function.
     *
     * @param message Pointer to the CRSF message header structure containing the message data.
     */
    virtual void handleMessage(const crsf_header_t * message) = 0;

    /**
     * Processes an incoming CRSF message received by the connector.
     *
     * This function handles CRSF messages by routing them to the appropriate destination,
     * processing them if necessary, or forwarding them to other connectors as required.
     * It distinguishes between standard and extended header messages and manages broadcast
     * and device-specific communications.
     *
     * @param connector Pointer to the CRSFConnector that received the message.
     * @param message Pointer to the CRSF message header structure containing the message data.
     */
    void processMessage(CRSFConnector *connector, const crsf_header_t *message);

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

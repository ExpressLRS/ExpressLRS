#ifndef CRSF_ROUTER_H
#define CRSF_ROUTER_H

#include "CRSFConnector.h"
#include "CRSFEndpoint.h"
#include "crc.h"
#include "msp.h"

#include <vector>

class CRSFRouter final
{
public:
    CRSFRouter() = default;
    ~CRSFRouter() = default;

    /**
     * Adds a CRSFConnector instance to the list of connectors managed by this CRSFRouter.
     *
     * Connectors are the ports that communicate with other devices in the network.
     *
     * @param connector Pointer to the CRSFConnector that will be added to the endpoint's managed connectors.
     */
    void addConnector(CRSFConnector *connector);

    /**
     * Removes a CRSFConnector instance from the list of connectors managed by this CRSFRouter.
     *
     * @param connector Pointer to the CRSFConnector that will be removed from the endpoint's managed connectors.
     */
    void removeConnector(CRSFConnector * connector);

    /**
     * Adds a CRSFEndpoint instance to the list of endpoints managed by this CRSFRouter.
     *
     * Endpoints are the targets of messages and are expected to process the messages that are sent to them.
     *
     * @param endpoint Pointer to the CRSFEndpoint that will be added to the endpoint's managed endpoints.
     */
    void addEndpoint(CRSFEndpoint *endpoint);

    /**
     * Processes a CRSF message received by a connector and determines the appropriate actions.
     *
     * This function evaluates the received message to determine whether it should be handled
     * directly, routed to another device, or delivered to one of the connectors managed by
     * the CRSFRouter instance. It ensures proper routing of messages based on their type
     * and destination address, including device-based and broadcast scenarios.
     *
     * @param connector Pointer to the CRSFConnector that received the message.
     * @param message Pointer to the CRSF message header structure containing the message data.
     */
    void processMessage(CRSFConnector *connector, const crsf_header_t *message) const;

    /**
     * Routes a CRSF message received by a connector to its appropriate destination.
     *
     * This function determines whether the message is device-specific or a broadcast message
     * and forwards it accordingly. For device-specific messages with a known destination address,
     * the message is delivered to the connector responsible for that address. For broadcast messages
     * or messages with an unknown destination, the message is forwarded to all other connectors.
     *
     * @param connector Pointer to the CRSFConnector that received the message.
     * @param message Pointer to the CRSF message header structure containing the message data.
     */
    void deliverMessage(const CRSFConnector *connector, const crsf_header_t *message) const;

    /**
     * Sets the header fields and calculates the CRC for a CRSF frame.
     *
     * This function populates the provided CRSF frame header with the specified frame type,
     * size, and destination address. It then calculates and appends the CRC to the frame.
     *
     * @param frame Pointer to the CRSF frame header structure that will be populated and updated.
     * @param frameType The type of the CRSF frame to be set in the header.
     * @param frameSize The size of the CRSF frame, including payload and header fields (type and CRC).
     * @param destAddr The destination address for the CRSF frame, as defined in the CRSF addressing scheme.
     */
    void SetHeaderAndCrc(crsf_header_t *frame, crsf_frame_type_e frameType, uint8_t frameSize, crsf_addr_e destAddr);

    /**
     * Sets the extended header fields and calculates the CRC for a CRSF frame.
     *
     * This function populates the extended header of a CRSF frame with the specified frame type,
     * size, and destination address. It also sets the originating address using the device ID
     * and calculates the CRC for the frame.
     *
     * @param frame Pointer to the CRSF extended header structure that will be populated and updated.
     * @param frameType The type of the CRSF frame to be set in the extended header.
     * @param frameSize The size of the CRSF frame, including payload, header (type, destination, origin, and CRC fields).
     * @param destAddr The destination address for the CRSF frame, as defined in the CRSF addressing scheme.
     * @param origAddr
     */
    void SetExtendedHeaderAndCrc(crsf_ext_header_t *frame, crsf_frame_type_e frameType, uint8_t frameSize, crsf_addr_e destAddr, crsf_addr_e origAddr);

    /**
     * Constructs a CRSF link statistics packet and populates the provided buffer.
     *
     * @param buffer Pointer to the buffer where the constructed packet will be stored.
     *               It is the caller's responsibility to ensure that the buffer
     *               has sufficient size to hold the generated packet.
     */
    void makeLinkStatisticsPacket(uint8_t *buffer);

    /**
     * Constructs an MSPv2 request frame and initializes it with the provided function and payload.
     *
     * @param frame Pointer to the buffer where the MSPv2 request frame will be constructed.
     * @param function The MSP function code that identifies the type of request being made.
     * @param payload Pointer to the payload data that will be included in the request.
     * @param payloadLength The length of the payload data in bytes.
     */
    void SetMspV2Request(uint8_t *frame, uint16_t function, const uint8_t *payload, uint8_t payloadLength);

    /**
     * Adds an MSP (Multiwii Serial Protocol) message to be processed by the CRSFRouter.
     *
     * This method encapsulates and prepares the MSP message with the provided packet data,
     * destination, and origin, then sends it for further processing within the system. It ensures
     * the message adheres to the required CRSF frame structure and calculates the necessary CRC values.
     *
     * @param packet Pointer to the mspPacket_t structure containing the MSP message payload and its associated metadata.
     * @param destination The target address within the CRSF system to which the message is directed.
     * @param origin The identifier indicating the source of the MSP message within the CRSF system.
     */
    void AddMspMessage(const mspPacket_t *packet, uint8_t destination, uint8_t origin);

    uint8_t getConnectorMaxPacketSize(crsf_addr_e origin) const;

    GENERIC_CRC8 crsf_crc = GENERIC_CRC8(CRSF_CRC_POLY);

private:
    std::set<CRSFConnector *> connectors;
    std::set<CRSFEndpoint *> endpoints;
};

// The global instance of the endpoint
extern CRSFRouter crsfRouter;
extern elrsLinkStatistics_t linkStats;

#endif //CRSF_ROUTER_H

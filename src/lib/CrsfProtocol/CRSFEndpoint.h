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
     * Processes a CRSF message received by a connector and determines the appropriate actions.
     *
     * This function evaluates the received message to determine whether it should be handled
     * directly, routed to another device, or delivered to one of the connectors managed by
     * the CRSFEndpoint instance. It ensures proper routing of messages based on their type
     * and destination address, including device-based and broadcast scenarios.
     *
     * @param connector Pointer to the CRSFConnector that received the message.
     * @param message Pointer to the CRSF message header structure containing the message data.
     */
    void processMessage(CRSFConnector *connector, const crsf_header_t *message);

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
     */
    void SetExtendedHeaderAndCrc(crsf_ext_header_t *frame, crsf_frame_type_e frameType, uint8_t frameSize, crsf_addr_e destAddr);

    /**
     * Constructs a CRSF link statistics packet and populates the provided buffer.
     *
     * @param buffer Pointer to the buffer where the constructed packet will be stored.
     *               It is the caller's responsibility to ensure that the buffer
     *               has sufficient size to hold the generated packet.
     * @param destination The CRSF address of the receiving device to include
     *                    in the packet header.
     */
    void makeLinkStatisticsPacket(uint8_t *buffer, crsf_addr_e destination);

    /**
     * Constructs an MSPv2 request frame and initializes it with the provided function and payload.
     *
     * @param frame Pointer to the buffer where the MSPv2 request frame will be constructed.
     * @param function The MSP function code that identifies the type of request being made.
     * @param payload Pointer to the payload data that will be included in the request.
     * @param payloadLength The length of the payload data in bytes.
     */
    void SetMspV2Request(uint8_t *frame, uint16_t function, uint8_t *payload, uint8_t payloadLength);

    /**
     * Adds an MSP (Multiwii Serial Protocol) message to be processed by the CRSFEndpoint.
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

    GENERIC_CRC8 crsf_crc = GENERIC_CRC8(CRSF_CRC_POLY);

    elrsLinkStatistics_t linkStats = {};

private:
    crsf_addr_e device_id;
    std::vector<CRSFConnector *> connectors;
};

// The global instance of the endpoint
extern CRSFEndpoint *crsfEndpoint;

#endif //CRSF_ENDPOINT_H

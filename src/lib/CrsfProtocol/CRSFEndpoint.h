#ifndef CRSF_ENDPOINT_H
#define CRSF_ENDPOINT_H

#include "CRSFConnector.h"

class CRSFEndpoint {
public:
    explicit CRSFEndpoint(const crsf_addr_e device_id)
        : device_id(device_id) {}

    virtual ~CRSFEndpoint() = default;

    /**
     * Retrieves the unique device identifier assigned to this CRSF endpoint.
     *
     * @return The device ID as a value of the crsf_addr_e enumeration.
     */
    crsf_addr_e getDeviceId() const { return device_id; }

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
     * or by matching the broadcast address, will be passed to this function.
     *
     * @param message Pointer to the CRSF message header structure containing the message data.
     */
    virtual void handleMessage(const crsf_header_t * message) = 0;

private:
    crsf_addr_e device_id;
};

#endif //CRSF_ENDPOINT_H

#ifndef CRSF_CONNECTOR_H
#define CRSF_CONNECTOR_H

#include "crsf_protocol.h"

#include <set>

/**
 * @class CRSFConnector
 *
 * @brief Abstract interface for integrating and managing connected CRSF devices.
 *
 * This class provides a mechanism to register devices that are reachable from this connector.
 * Derived classes must implement the pure virtual method `forwardMessage` to handle
 * the specifics of message forwarding.
 */
class CRSFConnector {
public:
    CRSFConnector() = default;
    virtual ~CRSFConnector() = default;

    /**
     * @brief Adds a device that is reachable via the connector to it's known device list.
     *
     * @param device_id The CRSF address of the device to add.
     */
    void addDevice(crsf_addr_e device_id);

    /**
     * @brief Checks whether the connector is responsible for forwarding messages
     * to a specified CRSF device.
     *
     * @param device_id The CRSF address of the device to check.
     * @return True if the connector forwards messages to the specified device, false otherwise.
     */
    bool forwardsTo(crsf_addr_e device_id);

    /**
     * @brief Forwards a CRSF message to its destination device.
     *
     * This pure virtual method should be implemented by derived classes
     * to handle the specifics of forwarding a CRSF message to the intended device.
     *
     * It is up to the implementation how it performs the actual forwarding.
     * For some connectors, they may translate the message to a different format or
     * may decide not to forward the message at all.
     *
     * @param message A pointer to the CRSF message header structure to be forwarded.
     */
    virtual void forwardMessage(const crsf_header_t *message) = 0;

    /**
     * @brief Logs debugging information for a CRSF message.
     *
     * This method outputs details about a given CRSF message, including
     * the device address, frame size, and type. If the message type
     * corresponds to an extended header frame (e.g., `CRSF_FRAMETYPE_DEVICE_PING` or higher),
     * additional fields such as destination and origin addresses are also logged.
     *
     * @param str A prefix or context string to include in the debug output.
     * @param message A pointer to the CRSF message header structure containing the data to be logged.
     */
    static void debugCRSF(const char * str, const crsf_header_t * message);

private:
    std::set<crsf_addr_e> devices;
};

#endif //CRSF_CONNECTOR_H

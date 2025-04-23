#include "OTAConnector.h"

#include "CRSF.h"

OTAConnector::OTAConnector()
{
    // add the devices that we know are reachable via this connector
    addDevice(CRSF_ADDRESS_CRSF_RECEIVER);
    addDevice(CRSF_ADDRESS_FLIGHT_CONTROLLER);
}

void OTAConnector::forwardMessage(crsf_ext_header_t *message)
{
    const uint8_t length = message->frame_size + 2;
    CRSF::AddMspMessage(length, (uint8_t *)message);
}


#include "CRSFConnector.h"

void CRSFConnector::addDevice(crsf_addr_e device_id)
{
    devices.insert(device_id);
}

void CRSFConnector::processMessage(crsf_ext_header_t *message)
{
    const crsf_frame_type_e packetType = message->type;
    if (packetType <= CRSF_FRAMETYPE_DEVICE_PING || message->dest_addr == CRSF_ADDRESS_BROADCAST || devices.find(message->dest_addr) != devices.end())
    {
        forwardMessage(message);
    }
}

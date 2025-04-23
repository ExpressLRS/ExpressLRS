#include "CRSFEndPoint.h"

void CRSFEndPoint::addConnector(CRSFConnector *connector)
{
    connectors.push_back(connector);
}

void CRSFEndPoint::processMessage(CRSFConnector *connector, crsf_ext_header_t *message)
{
    connector->addDevice(message->orig_addr);

    const crsf_frame_type_e packetType = message->type;
    if (packetType < CRSF_FRAMETYPE_DEVICE_PING || (packetType >= CRSF_FRAMETYPE_DEVICE_PING && (message->dest_addr == device_id || message->dest_addr == CRSF_ADDRESS_BROADCAST)))
    {
        const auto eat_message = handleMessage(message);
        if (eat_message)
        {
            return;
        }
    }

    if (packetType < CRSF_FRAMETYPE_DEVICE_PING || message->dest_addr == CRSF_ADDRESS_BROADCAST || message->dest_addr != device_id)
    {
        // forward the message to all other connectors and let them figure out if they want to deliver it
        for (const auto other : connectors)
        {
            if (other != connector) other->processMessage(message);
        }
    }
}

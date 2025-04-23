#include "HandsetConnector.h"

#include "CRSF.h"

HandsetConnector::HandsetConnector()
{
    // pre-seed the connector with devices which we know are directly connected
    addDevice(CRSF_ADDRESS_ELRS_LUA);
    addDevice(CRSF_ADDRESS_RADIO_TRANSMITTER);
}

void HandsetConnector::forwardMessage(crsf_ext_header_t *message)
{
    // append the message to queue for sending
    //FIXME
    const uint8_t length = message->frame_size + 2;
    CRSF::AddMspMessage(length, (uint8_t *)message);
}
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
    // FIXME
    // append the message to the queue for sending to the handset
}

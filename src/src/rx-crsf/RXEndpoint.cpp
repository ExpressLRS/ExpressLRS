#include "RXEndpoint.h"

RXEndpoint::RXEndpoint()
    : CRSFEndpoint(CRSF_ADDRESS_CRSF_RECEIVER)
{
}

bool RXEndpoint::handleMessage(crsf_ext_header_t *message)
{
    // FIXME implement all the messages that the RX will process
    return false;
}

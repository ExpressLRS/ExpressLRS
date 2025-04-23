#ifndef CRSF_CONNECTOR_H
#define CRSF_CONNECTOR_H

#include "crsf_protocol.h"

#include <set>

class CRSFConnector {
public:
    CRSFConnector() {}

    void addDevice(crsf_addr_e device_id);

    void processMessage(crsf_ext_header_t *message);

    virtual void forwardMessage(crsf_ext_header_t *message) = 0;

private:
    std::set<crsf_addr_e> devices;
};

#endif //CRSF_CONNECTOR_H

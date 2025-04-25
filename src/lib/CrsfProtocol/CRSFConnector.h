#ifndef CRSF_CONNECTOR_H
#define CRSF_CONNECTOR_H

#include "crsf_protocol.h"

#include <set>

class CRSFConnector {
public:
    CRSFConnector() {}
    virtual ~CRSFConnector() = default;

    void addDevice(crsf_addr_e device_id);

    bool forwardsTo(crsf_addr_e device_id);

    virtual void forwardMessage(const crsf_header_t *message) = 0;

    static void debugCRSF(const char * str, const crsf_header_t * message);

private:
    std::set<crsf_addr_e> devices;
};

#endif //CRSF_CONNECTOR_H

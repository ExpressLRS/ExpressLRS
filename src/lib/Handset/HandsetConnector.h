#ifndef HANDSET_CONNECTOR_H
#define HANDSET_CONNECTOR_H
#include "CRSFConnector.h"

class HandsetConnector : public CRSFConnector {
public:
    HandsetConnector();
    void forwardMessage(crsf_ext_header_t *message) override;
};

#endif //HANDSET_CONNECTOR_H

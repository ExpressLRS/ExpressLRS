#ifndef OTA_CONNECTOR_H
#define OTA_CONNECTOR_H
#include "CRSFEndPoint.h"

class OTAConnector final : public CRSFConnector {
public:
    OTAConnector();

    void forwardMessage(crsf_ext_header_t *message) override;
};

#endif //OTA_CONNECTOR_H

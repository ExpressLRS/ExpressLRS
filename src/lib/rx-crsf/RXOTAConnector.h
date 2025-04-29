#ifndef RX_OTA_CONNECTOR_H
#define RX_OTA_CONNECTOR_H
#include "CRSFConnector.h"

class RXOTAConnector final : public CRSFConnector {
public:
    RXOTAConnector();
    void forwardMessage(const crsf_header_t *message) override;
};

#endif //RX_OTA_CONNECTOR_H

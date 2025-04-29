#ifndef RX_ENDPOINT_H
#define RX_ENDPOINT_H
#include "CRSFEndpoint.h"

class RXEndpoint final : public CRSFEndpoint {
public:
    RXEndpoint();
    bool handleRaw(const crsf_header_t *message) override;
    void handleMessage(const crsf_header_t *message) override;
};

#endif //RX_ENDPOINT_H

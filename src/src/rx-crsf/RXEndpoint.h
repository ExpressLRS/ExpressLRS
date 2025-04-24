#ifndef RX_ENDPOINT_H
#define RX_ENDPOINT_H
#include "CRSFEndpoint.h"

class RXEndpoint final : public CRSFEndpoint {
public:
    RXEndpoint();
    bool handleMessage(crsf_ext_header_t *message) override;
};

#endif //RX_ENDPOINT_H
